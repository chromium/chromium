// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_consumer.h"

@protocol PasswordPickerViewControllerPresentationDelegate;
@protocol TableViewFaviconDataSource;

// Screen that presents a list of password credential groups for passwords that
// have more than 1 affiliated group.
@interface PasswordPickerViewController
    : LegacyChromeTableViewController <PasswordPickerConsumer>

// Delegate for handling dismissal of the view.
@property(nonatomic, weak) id<PasswordPickerViewControllerPresentationDelegate>
    delegate;

// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_VIEW_CONTROLLER_H_
