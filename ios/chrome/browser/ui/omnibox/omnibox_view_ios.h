// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/toolbar_model.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_left_image_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"

struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteTextFieldDelegate;
class GURL;
@class OmniboxTextFieldIOS;
@class OmniboxTextFieldPasteDelegate;
class WebOmniboxEditController;
@class OmniboxClearButtonBridge;

namespace ios {
class ChromeBrowserState;
}

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxView,
                       public OmniboxPopupViewSuggestionsDelegate {
 public:
  // Retains |field|.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 WebOmniboxEditController* controller,
                 id<OmniboxLeftImageConsumer> left_image_consumer,
                 ios::ChromeBrowserState* browser_state);
  ~OmniboxViewIOS() override;

  void SetPopupProvider(OmniboxPopupProvider* provider) {
    popup_provider_ = provider;
  };

  // Returns a color representing |security_level|, adjusted based on whether
  // the browser is in Incognito mode.
  static UIColor* GetSecureTextColor(
      security_state::SecurityLevel security_level,
      bool in_dark_mode);

  // OmniboxView implementation.
  void OpenMatch(const AutocompleteMatch& match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t selected_line,
                 base::TimeTicks match_selection_timestamp) override;
  base::string16 GetText() const override;
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  bool OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            size_t user_text_length) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  bool IsImeComposing() const override;
  bool IsIndicatingQueryRefinement() const override;

  // OmniboxView stubs.
  void Update() override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(base::string16::size_type* start,
                          base::string16::size_type* end) const override;
  void SelectAll(bool reversed) override {}
  void SetFocus() override {}
  void ApplyCaretVisibility() override {}
  void OnInlineAutocompleteTextCleared() override {}
  void OnRevertTemporaryText() override {}
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  int GetTextWidth() const override;
  int GetWidth() const override;

  // AutocompleteTextFieldDelegate methods
  void OnDidBeginEditing();
  bool OnWillChange(NSRange range, NSString* new_text);
  void OnDidChange(bool processing_user_input);
  void OnDidEndEditing();
  void OnAccept();
  void OnClear();
  bool OnCopy();
  void WillPaste();
  void OnDeleteBackward();

  // OmniboxPopupViewSuggestionsDelegate methods

  void OnTopmostSuggestionImageChanged(
      AutocompleteMatchType::Type type) override;
  void OnResultsChanged(const AutocompleteResult& result) override;
  void OnPopupDidScroll() override;
  void OnSelectedMatchForAppending(const base::string16& str) override;
  void OnSelectedMatchForOpening(AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const base::string16& pasted_text,
                                 size_t index) override;

  ios::ChromeBrowserState* browser_state() { return browser_state_; }

  // Updates this edit view to show the proper text, highlight and images.
  void UpdateAppearance();

  // Clears the text from the omnibox.
  void ClearText();

  // Hide keyboard and call OnDidEndEditing.  This dismisses the keyboard and
  // also finalizes the editing state of the omnibox.
  void HideKeyboardAndEndEditing();

  // Hide keyboard only.  Used when omnibox popups grab focus but editing isn't
  // complete.
  void HideKeyboard();

  // Focus the omnibox field.  This is used when the omnibox popup copies a
  // search query to the omnibox so the user can modify it further.
  // This does not affect the popup state and is a NOOP if the omnibox is
  // already focused.
  void FocusOmnibox();

  // Returns |true| if AutocompletePopupView is currently open.
  BOOL IsPopupOpen();

  // Returns the resource ID of the icon to show for the current text. Takes
  // into account the security level of the page, and |offline_page|.
  int GetIcon(bool offline_page) const;

 protected:
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override;

 private:
  // Creates the clear text UIButton to be used as a right view of |field_|.
  void CreateClearTextIcon(bool is_incognito);

  // Updates the view to show the appropriate button (e.g. clear text or voice
  // search) on the right side of |field_|.
  void UpdateRightDecorations();

  // Calculates text attributes according to |display_text| and
  // returns them in an autoreleased object.
  NSAttributedString* ApplyTextAttributes(const base::string16& text);

  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& scheme_range) override;

  // Removes the query refinement chip from the omnibox.
  void RemoveQueryRefinementChip();

  // Returns true if user input should currently be ignored.  On iOS7,
  // modifying the contents of a text field while Siri is pending leads to a
  // UIKit crash.  In order to sidestep that crash, OmniboxViewIOS checks that
  // voice search is not pending before attempting to process user actions that
  // may modify text field contents.
  // TODO(crbug.com/303212): Remove this workaround once the crash is fixed.
  bool ShouldIgnoreUserInputDueToPendingVoiceSearch();

  ios::ChromeBrowserState* browser_state_;

  OmniboxTextFieldIOS* field_;
  __strong UIButton* clear_text_button_;

  __strong OmniboxClearButtonBridge* clear_button_bridge_;

  OmniboxTextFieldPasteDelegate* paste_delegate_;
  WebOmniboxEditController* controller_;  // weak, owns us
  __weak id<OmniboxLeftImageConsumer> left_image_consumer_;

  State state_before_change_;
  NSString* marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // TODO(rohitrao): This is a monster hack, needed because closing the popup
  // ends up inadvertently triggering a new round of autocomplete.  Fix the
  // underlying problem, which is that textDidChange: is called when closing the
  // popup, and then remove this hack.  b/5877366.
  BOOL ignore_popup_updates_;

  // iOS 10.3 fails to apply the strikethrough style unless an extra style is
  // also applied. See https://crbug.com/699702 for discussion.
  BOOL use_strikethrough_workaround_;

  // Bridges delegate method calls from |field_| to C++ land.
  AutocompleteTextFieldDelegate* field_delegate_;

  // Temporary pointer to the attributed display string, stored as color and
  // other emphasis attributes are applied by the superclass.
  NSMutableAttributedString* attributing_display_string_;

  OmniboxPopupProvider* popup_provider_;  // weak

  // A flag that is set whenever any input or copy/paste event happened in the
  // omnibox while it was focused. Used to count event "user focuses the omnibox
  // to view the complete URL and immediately defocuses it".
  BOOL omnibox_interacted_while_focused_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
