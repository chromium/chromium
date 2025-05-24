// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination_picker_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol FileDestinationPickerActionDelegate;

@interface FileDestinationPickerViewController
    : ChromeTableViewController <FileDestinationPickerConsumer>

@property(nonatomic, weak) id<FileDestinationPickerActionDelegate>
    actionDelegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_
