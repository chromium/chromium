// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_ACTION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Action delegate for SaveToPhotosSettingsAccountSelectionViewController,
// invoked when an action is performed by the user.
@protocol
    SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate <NSObject>

// Invoked when the user taps "Add account" in
// SaveToPhotosSettingsAccountSelectionViewController.
- (void)saveToPhotosSettingsAccountSelectionViewControllerAddAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_ACTION_DELEGATE_H_
