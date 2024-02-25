// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"

@protocol FamilyPickerViewControllerPresentationDelegate;

// Presents the list of Google Family members of a user.
@interface FamilyPickerViewController
    : LegacyChromeTableViewController <FamilyPickerConsumer>

// Delegate for handling dismissal of the view.
@property(nonatomic, weak) id<FamilyPickerViewControllerPresentationDelegate>
    delegate;

// Sets up the back button on the left side of the navigation bar.
- (void)setupLeftBackButton;

// Sets up the cancel button on the left side of the navigation bar.
- (void)setupLeftCancelButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_VIEW_CONTROLLER_H_
