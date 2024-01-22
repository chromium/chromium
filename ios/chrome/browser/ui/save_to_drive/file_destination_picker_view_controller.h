// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_consumer.h"

@protocol FileDestinationPickerActionDelegate;

@interface FileDestinationPickerViewController
    : ChromeTableViewController <FileDestinationPickerConsumer>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@property(nonatomic, weak) id<FileDestinationPickerActionDelegate>
    actionDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_VIEW_CONTROLLER_H_
