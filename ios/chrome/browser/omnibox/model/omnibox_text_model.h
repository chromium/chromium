// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/common/omnibox_focus_state.h"

// Represents the changes between two OmniboxTextState objects. This is used by
// the controller to determine how its internal state should be updated after
// the view state changes.
struct OmniboxStateChanges {
  // `old_text` and `new_text` are not owned.
  raw_ptr<const std::u16string> old_text;
  raw_ptr<const std::u16string> new_text;
  size_t new_sel_start;
  size_t new_sel_end;
  bool selection_differs;
  bool text_differs;
  bool just_deleted_text;
};

enum class OmniboxPasteState {
  kNone,     // Most recent edit was not a paste.
  kPasting,  // In the middle of doing a paste.
  kPasted,   // Most recent edit was a paste.
};

struct OmniboxTextState {
  std::u16string text;  // The stored text.
  size_t sel_start = 0;  // selected text start index.
  size_t sel_end = 0;    // selected text end index.
};

// Manages the Omnibox text state.
struct OmniboxTextModel {
 public:
  OmniboxTextModel(OmniboxClient* client);
  ~OmniboxTextModel();

  // Sets the state of user_input_in_progress_. Returns whether said state
  // changed, so that the caller can evoke NotifyObserversInputInProgress().
  bool SetInputInProgressNoNotify(bool in_progress);

  // Checks if a focus state is active.
  bool HasFocus();

  /// Discards the focus state to None.
  void KillFocus();

  // If focus_state_ does not match `state`, we update it and notify the
  // InstantController about the change (passing along the `reason` for the
  // change).
  void SetFocusState(OmniboxFocusState state, OmniboxFocusChangeReason reason);

  // Called when the view is gaining focus.
  void OnSetFocus();

  // Updates the user text state.
  void UpdateUserText(const std::u16string& text);

  // Updates the model state and returns true if possible state changes occur,
  // returns false otherwise.
  bool UpdateStateAfterPossibleChange(const OmniboxStateChanges& state_changes);

  // Computes the State changes between two OmniboxTextState objects.
  OmniboxStateChanges GetStateChanges(const OmniboxTextState& before,
                                      const OmniboxTextState& after) const;

  // The Omnibox client.
  raw_ptr<OmniboxClient, DanglingUntriaged> omnibox_client;

  // The Omnibox focus state.
  OmniboxFocusState focus_state;
  // Whether the user input is in progress.
  bool user_input_in_progress;
  // The text that the user has entered. This does not include inline
  // autocomplete text that has not yet been accepted. `userText` can
  // contain a string without `userInputInProgress` being true.
  std::u16string user_text;
  // We keep track of when the user last focused on the omnibox.
  base::TimeTicks last_omnibox_focus;
  // Indicates whether the current interaction with the Omnibox resulted in
  // navigation (true), or user leaving the omnibox without taking any action
  // (false).
  // The value is initialized when the Omnibox receives focus and available for
  // use when the focus is about to be cleared.
  bool focus_resulted_in_navigation;
  // We keep track of when the user began modifying the omnibox text.
  // This should be valid whenever userInputInProgress is true.
  base::TimeTicks time_user_first_modified_omnibox;
  // Inline autocomplete is allowed if the user has not just deleted text. In
  // this case, `inlineAutocompletion` is appended to the `userText` and
  // displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text;
  // The inline autocompletion.
  std::u16string inline_autocompletion;
  // The Omnibox paste state.
  OmniboxPasteState paste_state;
  // The stored text state.
  OmniboxTextState text_state;
  // The input that was sent to the AutocompleteController. Since no
  // autocomplete query is started after a tab switch, it is possible for this
  // `input_` to differ from the one currently stored in AutocompleteController.
  AutocompleteInput input;
  // This is needed to properly update the SearchModel state when the user
  // presses escape.
  bool in_revert;
  // Used to know what should be displayed. Updated when e.g. the popup
  // selection changes, the results change, on navigation, on tab switch etc; it
  // should always be up-to-date.
  AutocompleteMatch current_match;
  // The initial text representing the current URL suitable for editing.
  std::u16string url_for_editing;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
