// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_

#import <string>

#import "base/time/time.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/common/omnibox_focus_state.h"

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
class OmniboxTextModel {
 public:
  OmniboxTextModel();
  ~OmniboxTextModel();

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
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
