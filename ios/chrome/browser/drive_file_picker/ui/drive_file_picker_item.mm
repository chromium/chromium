// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

}  // namespace

@implementation DriveFilePickerItem

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle
                              icon:(UIImage*)icon
                              type:(DriveItemType)type {
  self = [super init];
  if (self) {
    _identifier = [identifier copy];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _icon = icon;
    _type = type;
    _enabled = YES;
    _shouldFetchIcon = NO;
    _titleRangeToEmphasize.location = NSNotFound;
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
                  subtitle:nil
                      icon:CustomSymbolWithPointSize(kMyDriveSymbol,
                                                     kIconPointSize)
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
                  subtitle:nil
                      icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                     kIconPointSize)
                      type:DriveItemType::kSharedDrives];
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
                  subtitle:nil
                      icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                      kIconPointSize)
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
                  subtitle:nil
                      icon:DefaultSymbolWithPointSize(kClockSymbol,
                                                      kIconPointSize)
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
                  subtitle:nil
                      icon:DefaultSymbolWithPointSize(kPersonTwoSymbol,
                                                      kIconPointSize)
                      type:DriveItemType::kSharedWithMe];
  });
  return item;
}

@end
