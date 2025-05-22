// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/omnibox/browser/location_bar_model.h"

class OmniboxClient;
@protocol OmniboxCommands;
class OmniboxControllerIOS;
class OmniboxEditModelIOS;
@protocol OmniboxFocusDelegate;
@class OmniboxTextController;
@class OmniboxTextFieldIOS;
class ProfileIOS;
@protocol ToolbarCommands;

// Wraps a UITextField and interfaces with the rest of the autocomplete system.
class OmniboxViewIOS {
 public:
  // Represents the changes between two State objects. This is used by the
  // model to determine how its internal state should be updated after the view
  // state changes. See OmniboxEditModelIOS::OnAfterPossibleChange().
  struct StateChanges {
    // `old_text` and `new_text` are not owned.
    raw_ptr<const std::u16string> old_text;
    raw_ptr<const std::u16string> new_text;
    size_t new_sel_start;
    size_t new_sel_end;
    bool selection_differs;
    bool text_differs;
    bool just_deleted_text;
  };

  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 std::unique_ptr<OmniboxClient> client,
                 ProfileIOS* profile,
                 id<OmniboxCommands> omnibox_focuser,
                 id<ToolbarCommands> toolbar_commands_handler);
  OmniboxViewIOS(const OmniboxViewIOS&) = delete;
  OmniboxViewIOS& operator=(const OmniboxViewIOS&) = delete;
  virtual ~OmniboxViewIOS();

  OmniboxEditModelIOS* model();
  const OmniboxEditModelIOS* model() const;

  OmniboxControllerIOS* controller();
  const OmniboxControllerIOS* controller() const;

  void SetOmniboxTextController(OmniboxTextController* controller) {
    omnibox_text_controller_ = controller;
  }

  // Returns the current selection.
  NSRange GetCurrentSelection() const { return current_selection_; }

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::u16string GetText() const;

  // The user text is the text the user has manually keyed in. When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const std::u16string& text);
  virtual void SetUserText(const std::u16string& text, bool update_popup);

  // Sets the window text and the caret position. `notify_text_changed` is true
  // if the model should be notified of the change. Clears the additional text.
  virtual void SetWindowTextAndCaretPos(const std::u16string& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed);

  // Sets the caret position. Removes any selection. Clamps the requested caret
  // position to the length of the current text.
  virtual void SetCaretPos(size_t caret_pos);

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup();

  // Closes the autocomplete popup, if it's open. The name `ClosePopup`
  // conflicts with the OSX class override as that has a base class that also
  // defines a method with that name.
  virtual void CloseOmniboxPopup();

  // Called when the inline autocomplete text in the model may have changed.
  // `user_text` is the portion of omnibox text the user typed.
  // `inline`_autocompletion` is the autocompleted part.
  virtual void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion);

  // Checkpoints the current edit state before an operation that might trigger
  // a new autocomplete run to open or modify the popup. Call this before
  // user-initiated edit actions that trigger autocomplete, but *not* for
  // automatic changes to the textfield that should not affect autocomplete.
  virtual void OnBeforePossibleChange();

  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().
  virtual bool OnAfterPossibleChange();

  // Sets the omnibox adjacent additional text label in the location bar view.
  virtual void SetAdditionalText(const std::u16string& text);

  // Fills `start` and `end` with the indexes of the current selection's bounds.
  // It is not guaranteed that `*start < *end`, as the selection can be
  // directed. If there is no selection, `start` and `end` will both be equal
  // to the current cursor position.
  virtual void GetSelectionBounds(std::u16string::size_type* start,
                                  std::u16string::size_type* end) const;

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

 private:
  friend class TestOmniboxViewIOS;
  // Tracks important state that may change between OnBeforePossibleChange() and
  // OnAfterPossibleChange().
  struct State {
    std::u16string text;
    size_t sel_start;
    size_t sel_end;
  };

  // Fills `state` with the current text state.
  void GetState(State* state);

  // Returns the delta between `before` and `after`.
  StateChanges GetStateChanges(const State& before, const State& after);

  // Internally invoked whenever the text changes in some way.
  virtual void TextChanged();

  std::unique_ptr<OmniboxControllerIOS> controller_;

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
