// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_provider.h"

struct AutocompleteMatch;
class GURL;
class OmniboxClient;
@protocol OmniboxCommands;
@protocol OmniboxFocusDelegate;
@class OmniboxTextController;
@class OmniboxTextFieldIOS;
class ProfileIOS;
@protocol ToolbarCommands;

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxView {
 public:
  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 std::unique_ptr<OmniboxClient> client,
                 ProfileIOS* profile,
                 id<OmniboxCommands> omnibox_focuser,
                 id<OmniboxFocusDelegate> focus_delegate,
                 id<ToolbarCommands> toolbar_commands_handler,
                 bool is_lens_overlay);

  ~OmniboxViewIOS() override;

  void SetPopupProvider(OmniboxPopupProvider* provider) {
    popup_provider_ = provider;
  }

  void SetOmniboxTextController(OmniboxTextController* controller) {
    omnibox_text_controller_ = controller;
  }

  // Hide keyboard and call OnDidEndEditing.  This dismisses the keyboard and
  // also finalizes the editing state of the omnibox.
  void EndEditing();

  // OmniboxView implementation.
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  bool IsImeComposing() const override;
  bool IsIndicatingQueryRefinement() const override;
  void SetAdditionalText(const std::u16string& text) override;

  // OmniboxView stubs.
  void Update() override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(std::u16string::size_type* start,
                          std::u16string::size_type* end) const override;
  void SelectAll(bool reversed) override {}
  void SetFocus(bool is_user_initiated) override {}
  void ApplyCaretVisibility() override {}
  void OnInlineAutocompleteTextCleared() override {}
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override {}
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;

  // OmniboxTextChange methods.

  // Called when the Omnibox text field starts editing
  void OnDidBeginEditing();
  // Called before the Omnibox text field changes. `new_text` will replace the
  // text currently in `range`. This should return true if the text change
  // should happen and false otherwise.
  // See -textField:shouldChangeCharactersInRange:replacementString: for more
  // details.
  bool OnWillChange(NSRange range, NSString* new_text);
  // Called after the Omnibox text field changes. `processing_user_input` holds
  // whether the change was user-initiated or programmatic.
  void OnDidChange(bool processing_user_input);
  // Called when the Omnibox text field should copy.
  void OnCopy();
  // Clear the Omnibox text.
  void ClearText();
  // Called when the Omnibox text field should paste.
  void WillPaste();
  // Called when the backspace button is pressed in the Omnibox text field.
  void OnDeleteBackward();
  // Called when autocomplete text is accepted. (e.g. tap on autocomplete text,
  // tap on left/right arrow key).
  void OnAcceptAutocomplete();
  // Called when accepting current input / default suggestion.
  void OnAccept();

  // OmniboxAutocompleteController interactions.
  void OnPopupDidScroll();
  void OnSelectedMatchForAppending(const std::u16string& str);

  void OnCallActionTap();

  // Updates this edit view to show the proper text, highlight and images.
  void UpdateAppearance();

  // Updates the appearance of popup to have proper text alignment.
  void UpdatePopupAppearance();

  void OnClear();

  // Hide keyboard only.  Used when omnibox popups grab focus but editing isn't
  // complete.
  void HideKeyboard();

  // Focus the omnibox field.  This is used when the omnibox popup copies a
  // search query to the omnibox so the user can modify it further.
  // This does not affect the popup state and is a NOOP if the omnibox is
  // already focused.
  void FocusOmnibox();

  // Returns `true` if AutocompletePopupView is currently open.
  BOOL IsPopupOpen();

 protected:
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}

 private:
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& scheme_range) override {}

  OmniboxTextFieldIOS* field_;

  // Delegate that manages the browser UI changes in response to omnibox being
  // focused and defocused.
  __weak id<OmniboxFocusDelegate> focus_delegate_;

  State state_before_change_;
  NSString* marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // TODO(crbug.com/379693750): This is a monster hack, needed because closing
  // the popup ends up inadvertently triggering a new round of autocomplete. Fix
  // the underlying problem, which is that textDidChange: is called when closing
  // the popup, and then remove this hack.
  BOOL ignore_popup_updates_;

  // Whether the popup was scrolled during this omnibox interaction.
  bool suggestions_list_scrolled_ = false;
  // Whether it's the lens overlay omnibox.
  bool is_lens_overlay_;

  raw_ptr<OmniboxPopupProvider> popup_provider_;  // weak

  /// Controller that will replace OmniboxViewIOS at the end of the refactoring
  /// crbug.com/390409559.
  __weak OmniboxTextController* omnibox_text_controller_;

  // Used to cancel clipboard callbacks if this is deallocated;
  base::WeakPtrFactory<OmniboxViewIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_VIEW_IOS_H_
