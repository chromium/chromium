// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller_delegate.h"

class PrefService;
@protocol PrivacyGuideURLUsageConsumer;

// Mediator for the Privacy Guide URL usage UI.
@interface PrivacyGuideURLUsageMediator
    : NSObject <PrivacyGuideURLUsageViewControllerDelegate>

// Consumer for mediator.
@property(nonatomic, weak) id<PrivacyGuideURLUsageConsumer> consumer;

// Designated initializer. All the parameters should not be null.
// `userPrefService`: preference service from the profile.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cleans up anything before mediator shuts down.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_URL_USAGE_MEDIATOR_H_
