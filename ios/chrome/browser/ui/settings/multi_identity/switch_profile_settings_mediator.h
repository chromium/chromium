// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_delegate.h"

class ChromeAccountManagerService;
@protocol SwitchProfileSettingsConsumer;

// Mediator for the tabs settings.
@interface SwitchProfileSettingsMediator
    : NSObject <SwitchProfileSettingsDelegate>

- (instancetype)initWithChromeAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  activeProfileName:(NSString*)activeProfileName
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<SwitchProfileSettingsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_MEDIATOR_H_
