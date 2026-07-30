// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_MUTATOR_H_

#import <Foundation/Foundation.h>

// Mutator for the Suggestions from Gemini settings ViewController.
@protocol SuggestionsFromGeminiMutator <NSObject>

// Informs the delegate that the user tapped on the link to manage connected
// apps.
- (void)didSelectManageConnectedApps;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_MUTATOR_H_
