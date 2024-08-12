// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_ITEM_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_ITEM_H_

#import <UIKit/UIKit.h>

enum class DriveItemType : NSUInteger {
  kFolder,
  kFile,
};

// Model object representing an drive item.
@interface DriveItem : NSObject

// Drive item identifier.
@property(nonatomic, readonly) NSString* identifier;

// Drive item type (folder/file).
@property(nonatomic, readonly) DriveItemType type;

// Drive item icon.
@property(nonatomic, readonly) UIImage* icon;

// Drive item title.
@property(nonatomic, readonly) NSString* title;

// Drive item creation date (in case of files it represents the last time the
// file was modified).
@property(nonatomic, readonly) NSString* creationDate;

// Convenience factory methods to create root drive items.
+ (instancetype)myDriveItem;
+ (instancetype)sharedDrivesItem;
+ (instancetype)computersItem;
+ (instancetype)starredItem;
+ (instancetype)recentItem;
+ (instancetype)sharedWithMeItem;

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                              icon:(UIImage*)icon
                      creationDate:(NSString*)creationDate
                              type:(DriveItemType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_ITEM_H_
