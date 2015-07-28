/**********************************************************************

  Audacity: A Digital Audio Editor

  PrefsDialog.cpp

  Joshua Haberman
  James Crook

*******************************************************************//**

\class PrefsDialog
\brief Dialog that shows the current PrefsPanel in a tabbed divider.

*//*******************************************************************/

#include "../Audacity.h"
#include "PrefsDialog.h"

#include <wx/defs.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/intl.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>

#include <wx/listbook.h>

#if wxCHECK_VERSION(2, 8, 4)
#include <wx/treebook.h>
#else
#include "../widgets/treebook.h"
#endif

#include "../AudioIO.h"
#include "../Experimental.h"
#include "../Project.h"
#include "../Prefs.h"
#include "../ShuttleGui.h"

#include "PrefsPanel.h"

#include "BatchPrefs.h"
#include "DevicePrefs.h"
#include "DirectoriesPrefs.h"
#include "EffectsPrefs.h"
#include "GUIPrefs.h"
#include "ImportExportPrefs.h"
#include "KeyConfigPrefs.h"
#include "LibraryPrefs.h"
#include "MousePrefs.h"
#ifdef EXPERIMENTAL_MODULE_PREFS
#include "ModulePrefs.h"
#endif
#include "PlaybackPrefs.h"
#include "ProjectsPrefs.h"
#include "QualityPrefs.h"
#include "RecordingPrefs.h"
#include "SpectrumPrefs.h"
#include "ThemePrefs.h"
#include "TracksPrefs.h"
#include "WarningsPrefs.h"
#include "WaveformPrefs.h"
#include "ExtImportPrefs.h"

#ifdef EXPERIMENTAL_MIDI_OUT
#include "MidiIOPrefs.h"
#endif

BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
   EVT_BUTTON(wxID_OK, PrefsDialog::OnOK)
   EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)
   EVT_TREE_KEY_DOWN(wxID_ANY, PrefsDialog::OnTreeKeyDown) // Handles key events when tree has focus
END_EVENT_TABLE()


class wxTreebookExt : public wxTreebook
{
public:
   wxTreebookExt( wxWindow *parent,
      wxWindowID id, const wxString &titlePrefix)
      : wxTreebook( parent, id )
      , mTitlePrefix(titlePrefix)
   {;};
   ~wxTreebookExt(){;};
   virtual int ChangeSelection(size_t n);
   virtual int SetSelection(size_t n);
   const wxString mTitlePrefix;
};


int wxTreebookExt::ChangeSelection(size_t n) {
   int i = wxTreebook::ChangeSelection(n);
   wxString Temp = GetPageText( n );
   ((wxDialog*)GetParent())->SetTitle( Temp );
   ((wxDialog*)GetParent())->SetName( Temp );
   return i;
};

int wxTreebookExt::SetSelection(size_t n)
{
   int i = wxTreebook::SetSelection(n);
   wxString Temp = wxString(mTitlePrefix) + GetPageText( n );
   ((wxDialog*)GetParent())->SetTitle( Temp );
   ((wxDialog*)GetParent())->SetName( Temp );
   return i;
}



PrefsDialog::Factories
&PrefsDialog::DefaultFactories()
{
   // To do, perhaps:  create this table by registration, without including each PrefsPanel
   // class... and thus allowing a plug-in protocol
   static DevicePrefsFactory devicePrefsFactory;
   static PlaybackPrefsFactory playbackPrefsFactory;
   static RecordingPrefsFactory recordingPrefsFactory;
#ifdef EXPERIMENTAL_MIDI_OUT
   static MidiIOPrefsFactory midiIOPrefsFactory;
#endif
   static QualityPrefsFactory qualityPrefsFactory;
   static GUIPrefsFactory guiPrefsFactory;
   static TracksPrefsFactory tracksPrefsFactory;
   static ImportExportPrefsFactory importExportPrefsFactory;
   static ExtImportPrefsFactory extImportPrefsFactory;
   static ProjectsPrefsFactory projectsPrefsFactory;
#if !defined(DISABLE_DYNAMIC_LOADING_FFMPEG) || !defined(DISABLE_DYNAMIC_LOADING_LAME)
   static LibraryPrefsFactory libraryPrefsFactory;
#endif
   static WaveformPrefsFactory waveformPrefsFactory;
   static SpectrumPrefsFactory spectrumPrefsFactory;
   static DirectoriesPrefsFactory directoriesPrefsFactory;
   static WarningsPrefsFactory warningsPrefsFactory;
   static EffectsPrefsFactory effectsPrefsFactory;
#ifdef EXPERIMENTAL_THEME_PREFS
   static ThemePrefsFactory themePrefsFactory;
#endif
   // static BatchPrefsFactory batchPrefsFactory;
   static KeyConfigPrefsFactory keyConfigPrefsFactory;
   static MousePrefsFactory mousePrefsFactory;
#ifdef EXPERIMENTAL_MODULE_PREFS
   static ModulePrefsFactory modulePrefsFactory;
#endif

   static PrefsNode nodes[] = {
      &devicePrefsFactory,
      &playbackPrefsFactory,
      &recordingPrefsFactory,
#ifdef EXPERIMENTAL_MIDI_OUT
      &midiIOPrefsFactory,
#endif
      &qualityPrefsFactory,
      &guiPrefsFactory,
      &tracksPrefsFactory,
      &importExportPrefsFactory,
      &extImportPrefsFactory,
      &projectsPrefsFactory,
#if !defined(DISABLE_DYNAMIC_LOADING_FFMPEG) || !defined(DISABLE_DYNAMIC_LOADING_LAME)
      &libraryPrefsFactory,
#endif
      &waveformPrefsFactory,
      &spectrumPrefsFactory,
      &directoriesPrefsFactory,
      &warningsPrefsFactory,
      &effectsPrefsFactory,
#ifdef EXPERIMENTAL_THEME_PREFS
      &themePrefsFactory,
#endif
      // &batchPrefsFactory,
      &keyConfigPrefsFactory,
      &mousePrefsFactory,
#ifdef EXPERIMENTAL_MODULE_PREFS
      &modulePrefsFactory,
#endif
   };

   static Factories factories(nodes, nodes + sizeof(nodes) / sizeof(nodes[0]));
   return factories;
}


PrefsDialog::PrefsDialog
  (wxWindow * parent, const wxString &titlePrefix, Factories &factories)
:  wxDialog(parent, wxID_ANY, wxString(_("Audacity Preferences")),
            wxDefaultPosition,
            wxDefaultSize,
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
, mFactories(factories)
, mTitlePrefix(titlePrefix)
{
   ShuttleGui S(this, eIsCreating);

   S.StartVerticalLay(true);
   {
      S.StartHorizontalLay(wxALIGN_LEFT | wxEXPAND, true);
      {
         mCategories = new wxTreebookExt(this, wxID_ANY, mTitlePrefix);
         S.Prop(1);
         S.AddWindow(mCategories, wxEXPAND);

         {
            typedef std::pair<int, int> IntPair;
            std::vector<IntPair> stack;
            int iPage = 0;
            for (Factories::const_iterator it = factories.begin(), end = factories.end();
               it != end; ++it, ++iPage)
            {
               const PrefsNode &node = *it;
               PrefsPanelFactory &factory = *node.pFactory;
               wxWindow *const w = factory.Create(mCategories);
               if (stack.empty())
                  // Parameters are: AddPage(page, name, IsSelected, imageId).
                  mCategories->AddPage(w, w->GetName(), false, 0);
               else {
                  IntPair &top = *stack.rbegin();
                  mCategories->InsertSubPage(top.first, w, w->GetName(), false, 0);
                  if (--top.second == 0) {
                     // Expand all nodes before the layout calculation
                     mCategories->ExpandNode(top.first, true);
                     stack.pop_back();
                  }
               }
               if (node.nChildren > 0)
                  stack.push_back(IntPair(iPage, node.nChildren));
            }
         }
      }
      S.EndHorizontalLay();
   }
   S.EndVerticalLay();

   S.AddStandardButtons(eOkButton | eCancelButton);

#if defined(__WXGTK__)
   mCategories->GetTreeCtrl()->EnsureVisible(mCategories->GetTreeCtrl()->GetRootItem());
#endif

//   mCategories->SetSizeHints(-1, -1, 790, 600);  // 790 = 800 - (border * 2)
   Layout();
   Fit();
   wxSize sz = GetSize();

   // Collapse nodes only after layout so the tree is wide enough
   {
      int iPage = 0;
      for (Factories::const_iterator it = factories.begin(), end = factories.end();
         it != end; ++it, ++iPage)
         mCategories->ExpandNode(iPage, it->expanded);
   }

   // This ASSERT used to limit us to 800 x 600.
   // However, we think screens have got bigger now, and that was a bit too restrictive.
   // The impetus for increasing the limit (before we ASSERT) was that this ASSERT
   // was firing with wxWidgets 3.0, which has slightly different sizer behaviour.
   wxASSERT_MSG(sz.x <= 1000 && sz.y <= 750, wxT("Preferences dialog exceeds max size"));

   if (sz.x > 800) {
      sz.x = 800;
   }

   if (sz.y > 600) {
      sz.y = 600;
   }

   // Set the minimum height to be slightly bigger than default, as fix for bug 161.
   // The magic number 7 was determined by Ed's experimentation.
   // Frankly, this is a hack to work around a bug in wxTreebook, and
   // will have to be revisited if we add another category to mCategories.
   // JKC later added a category and 20 onto the 7.
   SetSizeHints(sz.x, sz.y + 7 + 20, 800, 600);

   // Center after all that resizing, but make sure it doesn't end up
   // off-screen
   CentreOnParent();
}

PrefsDialog::~PrefsDialog()
{
}

int PrefsDialog::ShowModal()
{
   /* long is signed, size_t is unsigned. On some platforms they are different
    * lengths as well. So we must check that the stored category is both > 0
    * and within the possible range of categories, making the first check on the
    * _signed_ value to avoid issues when converting an unsigned one.
    */
   long selected = GetPreferredPage();
   if (selected < 0 || size_t(selected) >= mCategories->GetPageCount())
      selected = 0;  // clamp to available range of tabs
   mCategories->SetSelection(selected);

   return wxDialog::ShowModal();
}

void PrefsDialog::OnCancel(wxCommandEvent & WXUNUSED(event))
{
   RecordExpansionState();

   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      ((PrefsPanel *) mCategories->GetPage(i))->Cancel();
   }

   EndModal(false);
}

void PrefsDialog::OnTreeKeyDown(wxTreeEvent & event)
{
   if(event.GetKeyCode() == WXK_RETURN)
      OnOK(event);
   else
      event.Skip(); // Ensure standard behavior when enter is not pressed
}

void PrefsDialog::OnOK(wxCommandEvent & WXUNUSED(event))
{
   RecordExpansionState();

   // Validate all pages first
   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      PrefsPanel *panel = (PrefsPanel *) mCategories->GetPage(i);

      // The dialog doesn't end until all the input is valid
      if (!panel->Validate()) {
         mCategories->SetSelection(i);
         return;
      }
   }

   // Now apply the changes
   for (size_t i = 0; i < mCategories->GetPageCount(); i++) {
      PrefsPanel *panel = (PrefsPanel *) mCategories->GetPage(i);

      panel->Apply();
   }

   SavePreferredPage();

#if USE_PORTMIXER
   if (gAudioIO) {
      // We cannot have opened this dialog if gAudioIO->IsAudioTokenActive(),
      // per the setting of AudioIONotBusyFlag and AudioIOBusyFlag in
      // AudacityProject::GetUpdateFlags().
      // However, we can have an invalid audio token (so IsAudioTokenActive()
      // is false), but be monitoring.
      // If monitoring, have to stop the stream, so HandleDeviceChange() can work.
      // We could disable the Preferences command while monitoring, i.e.,
      // set AudioIONotBusyFlag/AudioIOBusyFlag according to monitoring, as well.
      // Instead allow it because unlike recording, for example, monitoring
      // is not clearly something that should prohibit opening prefs.
      // TODO: We *could* be smarter in this method and call HandleDeviceChange()
      // only when the device choices actually changed. True of lots of prefs!
      // As is, we always stop monitoring before handling the device change.
      if (gAudioIO->IsMonitoring())
      {
         gAudioIO->StopStream();
         while (gAudioIO->IsBusy())
            wxMilliSleep(100);
      }
      gAudioIO->HandleDeviceChange();
   }
#endif

   // LL:  wxMac can't handle recreating the menus when this dialog is still active,
   //      so AudacityProject::UpdatePrefs() or any of the routines it calls must
   //      not cause AudacityProject::RebuildMenuBar() to be executed.
   for (size_t i = 0; i < gAudacityProjects.GetCount(); i++) {
      gAudacityProjects[i]->UpdatePrefs();
   }

   gPrefs->Flush();
   EndModal(true);
}

void PrefsDialog::SelectPageByName(wxString pageName)
{
   size_t n = mCategories->GetPageCount();

   for (size_t i = 0; i < n; i++) {
      if (mCategories->GetPageText(i) == pageName) {
         mCategories->SetSelection(i);
         return;
      }
   }
}

int PrefsDialog::GetSelectedPage() const
{
   return mCategories->GetSelection();
}

GlobalPrefsDialog::GlobalPrefsDialog(wxWindow * parent, Factories &factories)
   : PrefsDialog(parent, _("Preferences: "), factories)
{
}

GlobalPrefsDialog::~GlobalPrefsDialog()
{
}

long GlobalPrefsDialog::GetPreferredPage()
{
   long prefscat = gPrefs->Read(wxT("/Prefs/PrefsCategory"), 0L);
   return prefscat;
}

void GlobalPrefsDialog::SavePreferredPage()
{
   gPrefs->Write(wxT("/Prefs/PrefsCategory"), (long)GetSelectedPage());
   gPrefs->Flush();
}

void PrefsDialog::RecordExpansionState()
{
   // Remember expansion state of the tree control
   {
      int iPage = 0;
      for (Factories::iterator it = mFactories.begin(), end = mFactories.end();
         it != end; ++it, ++iPage)
         it->expanded = mCategories->IsNodeExpanded(iPage);
   }
}
