// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol SafetyCheckServiceDelegate;
@class SafetyCheckTableViewController;

// The accessibility identifier of the privacy settings collection view.
extern NSString* const kSafetyCheckTableViewId;

// Delegate for presentation events related to
// SafetyCheckTableViewController.
@protocol SafetyCheckTableViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)safetyCheckTableViewControllerDidRemove:
    (SafetyCheckTableViewController*)controller;

@end

// Controller for the UI that allows the user to perform a safety check and
// take action using the results (if needed).
@interface SafetyCheckTableViewController
    : SettingsRootTableViewController <SafetyCheckConsumer>

// Presentation delegate.
@property(nonatomic, weak)
    id<SafetyCheckTableViewControllerPresentationDelegate>
        presentationDelegate;

// Handler for taps on items on the safety check page.
@property(nonatomic, weak) id<SafetyCheckServiceDelegate> serviceDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_TABLE_VIEW_CONTROLLER_H_
