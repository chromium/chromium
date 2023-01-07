// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class SafeBrowsingStandardProtectionViewController;
@protocol SafeBrowsingStandardProtectionViewControllerDelegate;

// Delegate used for presentation events related to the view controller.
@protocol SafeBrowsingStandardProtectionViewControllerPresentationDelegate

- (void)safeBrowsingStandardProtectionViewControllerDidRemove:
    (SafeBrowsingStandardProtectionViewController*)viewController;

@end

// View controller used in the Safe Browsing Standard Protection view.
@interface SafeBrowsingStandardProtectionViewController
    : SettingsRootTableViewController <SafeBrowsingStandardProtectionConsumer,
                                       SettingsControllerProtocol>

// Navigation controller.
@property(nonatomic, strong) UINavigationController* navigationController;

// Presentation delegate.
@property(nonatomic, weak)
    id<SafeBrowsingStandardProtectionViewControllerPresentationDelegate>
        presentationDelegate;
// Model delegate.
@property(nonatomic, weak)
    id<SafeBrowsingStandardProtectionViewControllerDelegate>
        modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_H_
