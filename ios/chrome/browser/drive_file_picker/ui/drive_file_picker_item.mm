// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

NSString* const kDriveFilePickerMyDriveItemIdentifier =
    @"kDriveFilePickerMyDriveItemIdentifier";
NSString* const kDriveFilePickerSharedDrivesItemIdentifier =
    @"kDriveFilePickerSharedDrivesItemIdentifier";
NSString* const kDriveFilePickerComputersItemIdentifier =
    @"kDriveFilePickerComputersItemIdentifier";
NSString* const kDriveFilePickerStarredItemIdentifier =
    @"kDriveFilePickerStarredItemIdentifier";
NSString* const kDriveFilePickerRecentItemIdentifier =
    @"kDriveFilePickerRecentItemIdentifier";
NSString* const kDriveFilePickerSharedWithMeItemIdentifier =
    @"kDriveFilePickerSharedWithMeItemIdentifier";

}  // namespace

@implementation DriveFilePickerItem

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                              icon:(UIImage*)icon
                      creationDate:(NSString*)creationDate
                              type:(DriveItemType)type {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _title = title;
    _icon = icon;
    _creationDate = creationDate;
    _type = type;
    _enabled = YES;
  }
  return self;
}

+ (instancetype)myDriveItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  // TODO(crbug.com/344812548): Add a11y title.
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerMyDriveItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_MY_DRIVE)
                      icon:CustomSymbolWithPointSize(kMyDriveSymbol,
                                                     kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kMyDrive];
  });
  return item;
}

+ (instancetype)sharedDrivesItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerSharedDrivesItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_SHARED_DRIVES)
                      icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                     kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kSharedDrives];
  });
  return item;
}

+ (instancetype)computersItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerComputersItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_COMPUTERS)
                      icon:DefaultSymbolWithPointSize(kLaptopAndIphoneSymbol,
                                                      kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kComputers];
  });
  return item;
}

+ (instancetype)starredItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerStarredItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_STARRED)
                      icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                      kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kStarred];
  });
  return item;
}

+ (instancetype)recentItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerRecentItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_RECENT)
                      icon:DefaultSymbolWithPointSize(kClockSymbol,
                                                      kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kRecent];
  });
  return item;
}

+ (instancetype)sharedWithMeItem {
  static DriveFilePickerItem* item;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    item = [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerSharedWithMeItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_SHARED_WITH_ME)
                      icon:DefaultSymbolWithPointSize(kPersonTwoSymbol,
                                                      kIconPointSize)
              creationDate:nil
                      type:DriveItemType::kSharedWithMe];
  });
  return item;
}

@end
