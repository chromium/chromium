// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ITEM_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ITEM_H_

#import <UIKit/UIKit.h>

enum class DriveItemType : NSUInteger {
  kFolder,
  kFile,
  kMyDrive,
  kSharedDrives,
  kComputers,
  kStarred,
  kRecent,
  kSharedWithMe,
};

// Model object representing a drive item.
@interface DriveFilePickerItem : NSObject

// Drive item identifier.
@property(nonatomic, readonly) NSString* identifier;

// Drive item type (folder/file).
@property(nonatomic, readonly) DriveItemType type;

// Drive item icon.
@property(nonatomic, strong) UIImage* icon;

// Drive item title.
@property(nonatomic, readonly) NSString* title;

// Drive item subtitle.
@property(nonatomic, readonly) NSString* subtitle;

// Range of the title which should be emphasized.
@property(nonatomic, assign) NSRange titleRangeToEmphasize;

// Whether this item is enabled. YES by default.
@property(nonatomic, assign) BOOL enabled;

// Convenience factory methods to create root drive items.
+ (instancetype)myDriveItem;
+ (instancetype)sharedDrivesItem;
+ (instancetype)computersItem;
+ (instancetype)starredItem;
+ (instancetype)recentItem;
+ (instancetype)sharedWithMeItem;

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle
                              icon:(UIImage*)icon
                              type:(DriveItemType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ITEM_H_
