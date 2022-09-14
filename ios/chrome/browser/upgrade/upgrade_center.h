// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_H_
#define IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/upgrade/upgrade_recommended_details.h"

@protocol ApplicationCommands;
@class UpgradeCenter;

namespace infobars {
class InfoBarManager;
}

@protocol UpgradeCenterClient

// This is expected to call -addInfoBarToHelper:forTabId: for each tab to place
// the infobars in the UI. The client must not unregister itself while in this
// method.
- (void)showUpgrade:(UpgradeCenter*)center;

@end

@interface UpgradeCenter : NSObject

// Returns the singleton instance of the class.
+ (UpgradeCenter*)sharedInstance;

// Registers a client and a `dispatcher` for the UpgradeCenter. Client and
// `dispatcher` are not retained, unregisterClient: must be called before
// the object goes away.
- (void)registerClient:(id<UpgradeCenterClient>)client
           withHandler:(id<ApplicationCommands>)handler;

// Unregisters a client.
- (void)unregisterClient:(id<UpgradeCenterClient>)client;

// Clients should call this method when -showUpgrade: is called or when a new
// tab is created. The infobar will not be created if it already exists or if
// there is no need to do so.
- (void)addInfoBarToManager:(infobars::InfoBarManager*)infoBarManager
                   forTabId:(NSString*)tabId;

// For the UpgradeCenter to make the distinction between an infobar closed by
// the user directly and an infobar dismissed because the Tab it is on is
// removed.
- (void)tabWillClose:(NSString*)tabId;

// Called when a notification is received from one of the upgrade mechanism.
- (void)upgradeNotificationDidOccur:(const UpgradeRecommendedDetails&)details;

@end

@interface UpgradeCenter (UsedForTests)
// Reset everything (forget clients, remove the infobar everywhere...)
- (void)resetForTests;
// Simulate the minimum display interval having elapsed.
- (void)setLastDisplayToPast;
@end

#endif  // IOS_CHROME_BROWSER_UPGRADE_UPGRADE_CENTER_H_
