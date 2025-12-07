// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
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
@interface SafeBrowsingEnhancedProtectionViewController : UIViewController

// Presentation delegate.
@property(nonatomic, weak)
    id<SafeBrowsingEnhancedProtectionViewControllerPresentationDelegate>
        presentationDelegate;

// Handler for the Application commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_VIEW_CONTROLLER_H_
