// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SUGGESTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SUGGESTION_DELEGATE_H_

#import <UIKit/UIKit.h>

// Protocol for Gemini suggestion chips.
// TODO(crbug.com/454080484): Change "Delegate" naming to "Handling" since we
// can't pass the internal object as a parameter.
@protocol GeminiSuggestionDelegate

// Fetches the zero-state suggestion chips and returns them as a completion.
- (void)fetchZeroStateSuggestions:
    (void (^)(NSArray<NSString*>* suggestions))completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SUGGESTION_DELEGATE_H_
