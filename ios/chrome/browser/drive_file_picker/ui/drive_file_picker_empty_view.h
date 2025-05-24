// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

// A view to inform the user that a drive collection is empty.
@interface DriveFilePickerEmptyView : UIView

// Factory method to create an empty folder view.
+ (instancetype)emptyDriveFolderView;
// Factory method to create a no matching result view.
+ (instancetype)noMatchingResultView;

- (instancetype)initWithMessage:(NSString*)message
                     symbolName:(NSString*)symbolName NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_EMPTY_VIEW_H_
