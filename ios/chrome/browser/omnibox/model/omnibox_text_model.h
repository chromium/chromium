// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_

#import <Foundation/Foundation.h>

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
  size_t sel_start;     // selected text start index.
  size_t sel_end;       // selected text end idex.
};

// Manages the Omnibox text state.
@interface OmniboxTextModel : NSObject

/// The Omnibox focus state.
@property(nonatomic, assign) OmniboxFocusState focusState;
/// Whether the user input is in progress.
@property(nonatomic, assign) BOOL userInputInProgress;
/// The text that the user has entered.  This does not include inline
/// autocomplete text that has not yet been accepted.  `user_text_` can
/// contain a string without `user_input_in_progress_` being true.
@property(nonatomic, assign) std::u16string userText;
/// Used to know what should be displayed. Updated when e.g. the popup
/// selection changes, the results change, on navigation, on tab switch etc;
/// it should always be up-to-date.
@property(nonatomic, assign) AutocompleteMatch currentMatch;
/// We keep track of when the user last focused on the omnibox.
@property(nonatomic, assign) base::TimeTicks lastOmniboxFocus;
/// Indicates whether the current interaction with the Omnibox resulted in
/// navigation (true), or user leaving the omnibox without taking any action
/// (false).
/// The value is initialized when the Omnibox receives focus and available for
/// use when the focus is about to be cleared.
@property(nonatomic, assign) BOOL focusResultedInNavigation;
/// We keep track of when the user began modifying the omnibox text.
/// This should be valid whenever user_input_in_progress_ is true.
@property(nonatomic, assign) base::TimeTicks timeUserFirstModifiedOmnibox;
/// Inline autocomplete is allowed if the user has not just deleted text. In
/// this case, inline_autocompletion_ is appended to the user_text_ and
/// displayed selected (at least initially).
///
/// NOTE: When the popup is closed there should never be inline autocomplete
/// text (actions that close the popup should either accept the text, convert
/// it to a normal selection, or change the edit entirely).
@property(nonatomic, assign) BOOL justDeletedText;
/// The inile autocompletion.
@property(nonatomic, assign) std::u16string inlineAutocompletion;
/// The Omnibox paste state.
@property(nonatomic, assign) OmniboxPasteState pasteState;
/// Whether or not the text state is being reverted.
@property(nonatomic, assign) BOOL inRevert;
/// The input that was sent to the AutocompleteController.
@property(nonatomic, assign) AutocompleteInput input;
/// The stored text state.
@property(nonatomic, assign) OmniboxTextState textState;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_TEXT_MODEL_H_
