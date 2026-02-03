// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"

class AuthenticationService;
@protocol GeminiSettingsConsumer;
class PrefService;
@protocol SceneCommands;

// Gemini Mediator.
@interface GeminiSettingsMediator : NSObject <GeminiSettingsMutator>

// The scene commands handler for this mediator.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Usually the view controller.
@property(nonatomic, weak) id<GeminiSettingsConsumer> consumer;

// Designated initializer. All the parameters should not be null.
// 'authService': authentication service for the profile.
// `prefService`: preference service from the profile.
- (instancetype)initWithAuthService:(AuthenticationService*)authService
                        prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops observing objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SETTINGS_MEDIATOR_H_
