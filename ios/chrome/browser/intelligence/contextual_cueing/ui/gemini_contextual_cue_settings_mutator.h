// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator protocol for the Gemini contextual cue settings view controller to
// communicate user actions to its mediator.
@protocol GeminiContextualCueSettingsMutator <NSObject>

// Called when the user selects the "Manage Gemini suggestions" row.
- (void)didTapManageSuggestions;

// Called when the user selects the "Edit prompt" row.
- (void)didTapEditPrompt;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_MUTATOR_H_
