// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class SafeBrowsingEnhancedProtectionViewController;

// Delegate for presentation events related to
// Safe Browsing Enhanced Protection View Controller.
@protocol
    SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)safeBrowsingEnhancedProtectionViewControllerDidRemove:
    (SafeBrowsingEnhancedProtectionViewController*)controller;

@end

// View controller to related to Privacy safe browsing enhanced protection
// setting.
@interface SafeBrowsingEnhancedProtectionViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Navigation controller.
@property(nonatomic, strong) UINavigationController* navigationController;

// Presentation delegate.
@property(nonatomic, weak)
    id<SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_
