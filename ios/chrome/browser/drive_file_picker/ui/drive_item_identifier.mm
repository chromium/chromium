// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

}  // namespace

@implementation DriveItemIdentifier

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
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"root"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_MY_DRIVE)
                    icon:CustomSymbolWithPointSize(kMyDriveSymbol,
                                                   kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kMyDrive];
}

+ (instancetype)sharedDrivesItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"shared_drives"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_SHARED_DRIVES)
                    icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                   kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kSharedDrives];
}

+ (instancetype)computersItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"computers"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_COMPUTERS)
                    icon:DefaultSymbolWithPointSize(kLaptopAndIphoneSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kComputers];
}

+ (instancetype)starredItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"starred"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_STARRED)
                    icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kStarred];
}

+ (instancetype)recentItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"recent"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_RECENT)
                    icon:DefaultSymbolWithPointSize(kClockSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kRecent];
}

+ (instancetype)sharedWithMeItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"shared_with_me"
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_SHARED_WITH_ME)
                    icon:DefaultSymbolWithPointSize(kPersonTwoSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kSharedWithMe];
}

@end
