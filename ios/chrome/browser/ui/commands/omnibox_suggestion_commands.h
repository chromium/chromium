// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_OMNIBOX_SUGGESTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_OMNIBOX_SUGGESTION_COMMANDS_H_

#import <UIKit/UIKit.h>

// Commands to advance the suggestion highlight in the suggestions popup of the
// omnibox.
@protocol OmniboxSuggestionCommands<NSObject>

// Moves the highlight down.
- (void)highlightNextSuggestion;
// Moves the highlight up.
- (void)highlightPreviousSuggestion;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_OMNIBOX_SUGGESTION_COMMANDS_H_
