// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_mutator.h"

@protocol GeminiCommands;
@protocol GeminiContextualCueSettingsConsumer;
@class GeminiContextualCueSettingsMediator;
@protocol SettingsCommands;

namespace web {
class WebState;
}  // namespace web

// Delegate for `GeminiContextualCueSettingsMediator`.
@protocol GeminiContextualCueSettingsMediatorDelegate <NSObject>

// Requests that the coordinator dismiss the contextual cue settings modal and
// invoke `completion` once dismissed.
- (void)geminiContextualCueSettingsMediator:
            (GeminiContextualCueSettingsMediator*)mediator
              dismissSettingsWithCompletion:(ProceduralBlock)completion;

@end

// Mediator for the Gemini contextual cue settings bottom sheet.
@interface GeminiContextualCueSettingsMediator
    : NSObject <GeminiContextualCueSettingsMutator>

#pragma mark - Public

// Consumer that displays the contextual cue settings UI.
@property(nonatomic, weak) id<GeminiContextualCueSettingsConsumer> consumer;

// Delegate for coordinating dismissal of the settings sheet.
@property(nonatomic, weak) id<GeminiContextualCueSettingsMediatorDelegate>
    delegate;

// Initializes the mediator with the active `webState`, `geminiHandler`, and
// `settingsHandler`.
- (instancetype)initWithWebState:(web::WebState*)webState
                   geminiHandler:(id<GeminiCommands>)geminiHandler
                 settingsHandler:(id<SettingsCommands>)settingsHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator from its dependencies.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_MEDIATOR_H_
