// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CELL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class DriveFilePickerItem;

// Content configuration for a cell in the Drive file picker.
@interface DriveFilePickerCellContentConfiguration
    : NSObject <UIContentConfiguration>

// List content configuration.
@property(nonatomic, readonly)
    UIListContentConfiguration* listContentConfiguration;
// Whether the cell content should appear as enabled.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Returns the default configuration for a Drive file picker cell.
+ (instancetype)cellConfiguration;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_CELL_CONTENT_CONFIGURATION_H_
