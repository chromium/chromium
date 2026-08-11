// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol SuggestionsFromGeminiHelpImproveConsumer;
class PrefService;

// The Mediator for controlling the Suggestions from Gemini Help Improve
// settings.
@interface SuggestionsFromGeminiHelpImproveMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<SuggestionsFromGeminiHelpImproveConsumer>
    consumer;

// Initializes the mediator with a `PrefService`.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_MEDIATOR_H_
