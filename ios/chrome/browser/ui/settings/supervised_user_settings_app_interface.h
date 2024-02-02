// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SUPERVISED_USER_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SUPERVISED_USER_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "components/supervised_user/core/browser/supervised_user_utils.h"

// The app interface for supervised user settings tests.
@interface SupervisedUserSettingsAppInterface : NSObject

// Sets the parental control setting to filter websites for supervised users.
+ (void)setSupervisedUserURLFilterBehavior:
    (supervised_user::FilteringBehavior)behavior;

// Resets to default parental control settings for website filtering.
+ (void)resetSupervisedUserURLFilterBehavior;

// Removes the manually set blocklist/allowlist for a supervised user.
+ (void)resetManualUrlFiltering;

// Replaces (any) previous permission creator with a fake Permission Creator
// for a supervised user.
+ (void)setFakePermissionCreator;

// Approves a requested website for a supervised user.
+ (void)approveWebsiteDomain:(const NSURL*)url;

// Sets a supervised user's filtering behaviour to "Allow All Sites".
+ (void)setFilteringToAllowAllSites;

// Sets a supervised user's filtering behaviour to "Allow Approved Sites".
+ (void)setFilteringToAllowApprovedSites;

// Adds the `url` to a supervised user's allow-list.
+ (void)addWebsiteToAllowList:(NSURL*)host;

// Adds the `url` to a supervised user's block-list.
+ (void)addWebsiteToBlockList:(NSURL*)host;

// Marks the "first time banner" of the SU interstitial as not shown.
+ (void)resetFirstTimeBanner;

// Sets the behaviour of Safe Search filtering: it sets the default
// response of the ClassifyUrl endpoint to allow or restrict all requests.
// In order to use this method, use setUp/tearDown TestUrlLoaderFactoryHelper
// to set up the necessary dependencies.
+ (void)setDefaultClassifyURLNavigationIsAllowed:(BOOL)is_allowed;

// Creates a TestUrlLoaderFactoryHelper singleton.
+ (void)setUpTestUrlLoaderFactoryHelper;

// Tear down the TestUrlLoaderFactoryHelper singleton.
+ (void)tearDownTestUrlLoaderFactoryHelper;

// Returns the number of Supervised User interstitials attached to the existing
// web states.
+ (NSInteger)countSupervisedUserIntersitialsForExistingWebStates;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SUPERVISED_USER_SETTINGS_APP_INTERFACE_H_
