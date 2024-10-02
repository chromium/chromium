// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller_delegate.h"

class PrefService;
@protocol PrivacySafeBrowsingConsumer;
@protocol PrivacySafeBrowsingNavigationCommands;

// Mediator for the Privacy Safe Browsing.
@interface PrivacySafeBrowsingMediator
    : NSObject <PrivacySafeBrowsingViewControllerDelegate>

// View controller.
@property(nonatomic, weak) id<PrivacySafeBrowsingConsumer> consumer;
// Handler used to navigate inside the privacy safe browsing setting.
@property(nonatomic, weak) id<PrivacySafeBrowsingNavigationCommands> handler;
// Represents whether the Safe Browsing UI is being presented due to the user's
// interaction with a promo.
@property(nonatomic, assign) BOOL openedFromPromoInteraction;

// Designated initializer. All the parameters should not be null.
// `userPrefService`: preference service from the profile.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Handles logic for selecting an item in the view and the Privacy Safe Browsing
// No Protection pop up.
- (void)selectSettingItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_H_
