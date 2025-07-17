// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mutator.h"

@protocol ApplicationCommands;
@protocol BWGSettingsConsumer;
class PrefService;

// BWG Mediator.
@interface BWGSettingsMediator : NSObject <BWGSettingsMutator>

// The application command handler for this mediator.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// Usually the view controller.
@property(nonatomic, weak) id<BWGSettingsConsumer> consumer;

// Designated initializer. All the parameters should not be null.
// `prefService`: preference service from the profile.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops observing objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_MEDIATOR_H_
