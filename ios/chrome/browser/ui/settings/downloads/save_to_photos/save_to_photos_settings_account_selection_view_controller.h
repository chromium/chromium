// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate;
@protocol
    SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate;
@protocol SaveToPhotosSettingsMutator;

// Account selection view controller for Save to Photos settings.
@interface SaveToPhotosSettingsAccountSelectionViewController
    : SettingsRootTableViewController <
          SaveToPhotosSettingsAccountSelectionConsumer>

// The mutator is used by the view controller to mutate the data in the model.
@property(nonatomic, weak) id<SaveToPhotosSettingsMutator> mutator;

// When the view controller is dismissed, the presentation delegate is notified.
@property(nonatomic, weak)
    id<SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate>
        presentationDelegate;
// When some action is performed by the user which does not correspond to a
// change in the model data e.g. a button is tapped to open a sub-menu, the
// action delegate is notified.
@property(nonatomic, weak)
    id<SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate>
        actionDelegate;

// Initialization.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_SETTINGS_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
