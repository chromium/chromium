// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_base.h"

class OmniboxClient;
@protocol OmniboxCommands;
@protocol OmniboxFocusDelegate;
@class OmniboxTextController;
@class OmniboxTextFieldIOS;
class ProfileIOS;
@protocol ToolbarCommands;

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxViewBase {
 public:
  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 std::unique_ptr<OmniboxClient> client,
                 ProfileIOS* profile,
                 id<OmniboxCommands> omnibox_focuser,
                 id<ToolbarCommands> toolbar_commands_handler);

  ~OmniboxViewIOS() override;

  void SetOmniboxTextController(OmniboxTextController* controller) {
    omnibox_text_controller_ = controller;
  }

  // OmniboxView implementation.
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange() override;
  void SetAdditionalText(const std::u16string& text) override;
  void GetSelectionBounds(std::u16string::size_type* start,
                          std::u16string::size_type* end) const override;

  // OmniboxTextChange methods.

  // Called before the Omnibox text field changes. `new_text` will replace the
  // text currently in `range`. This should return true if the text change
  // should happen and false otherwise.
  // See -textField:shouldChangeCharactersInRange:replacementString: for more
  // details.
  bool OnWillChange(NSRange range, NSString* new_text);
  // Called after the Omnibox text field changes. `processing_user_input` holds
  // whether the change was user-initiated or programmatic.
  void OnDidChange(bool processing_user_input);
  // Called when autocomplete text is accepted. (e.g. tap on autocomplete text,
  // tap on left/right arrow key).
  void OnAcceptAutocomplete();

  // Returns the current selection.
  NSRange GetCurrentSelection() const { return current_selection_; }

 private:
  OmniboxTextFieldIOS* field_;

  State state_before_change_;
  NSString* marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // TODO(crbug.com/379693750): This is a monster hack, needed because closing
  // the popup ends up inadvertently triggering a new round of autocomplete. Fix
  // the underlying problem, which is that textDidChange: is called when closing
  // the popup, and then remove this hack.
  BOOL ignore_popup_updates_;

  /// Controller that will replace OmniboxViewIOS at the end of the refactoring
  /// crbug.com/390409559.
  __weak OmniboxTextController* omnibox_text_controller_;

  // Used to cancel clipboard callbacks if this is deallocated;
  base::WeakPtrFactory<OmniboxViewIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_
