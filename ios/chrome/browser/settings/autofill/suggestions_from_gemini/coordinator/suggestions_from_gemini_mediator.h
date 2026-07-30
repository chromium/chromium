// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_mutator.h"

@protocol SuggestionsFromGeminiConsumer;
class PrefService;

// The Mediator for controlling the Suggestions from Gemini settings.
@interface SuggestionsFromGeminiMediator
    : NSObject <SuggestionsFromGeminiMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<SuggestionsFromGeminiConsumer> consumer;

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_MEDIATOR_H_
