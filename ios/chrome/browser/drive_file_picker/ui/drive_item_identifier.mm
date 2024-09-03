// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

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
  }
  return self;
}

+ (instancetype)myDriveItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"root"
                   title:@"TEST My Drive"
                    icon:CustomSymbolWithPointSize(kMyDriveSymbol,
                                                   kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kMyDrive];
}

+ (instancetype)sharedDrivesItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"shared_drives"
                   title:@"TEST Shared Drives"
                    icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                   kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kSharedDrives];
}

+ (instancetype)computersItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"computers"
                   title:@"TEST Computers"
                    icon:DefaultSymbolWithPointSize(kLaptopAndIphoneSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kComputers];
}

+ (instancetype)starredItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"starred"
                   title:@"TEST Starred"
                    icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kStarred];
}

+ (instancetype)recentItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"recent"
                   title:@"TEST Recent"
                    icon:DefaultSymbolWithPointSize(kClockSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kRecent];
}

+ (instancetype)sharedWithMeItem {
  // TODO(crbug.com/344812548): Add a11y title and the corresponding identifier.
  return [[DriveItemIdentifier alloc]
      initWithIdentifier:@"shared_with_me"
                   title:@"TEST Shared With Me"
                    icon:DefaultSymbolWithPointSize(kPersonTwoSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kSharedWithMe];
}

@end
