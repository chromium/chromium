// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

}  // namespace

@implementation DriveItem

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
  return
      [[DriveItem alloc] initWithIdentifier:@"my_drive"
                                      title:@"TEST My Drive"
                                       icon:CustomSymbolWithPointSize(
                                                kMyDriveSymbol, kIconPointSize)
                               creationDate:nil
                                       type:DriveItemType::kFolder];
}

+ (instancetype)sharedDrivesItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItem alloc]
      initWithIdentifier:@"shared_drives"
                   title:@"TEST Shared Drives"
                    icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                   kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kFolder];
}

+ (instancetype)computersItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItem alloc]
      initWithIdentifier:@"computers"
                   title:@"TEST Computers"
                    icon:DefaultSymbolWithPointSize(kLaptopAndIphoneSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kFolder];
}

+ (instancetype)starredItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItem alloc]
      initWithIdentifier:@"starred"
                   title:@"TEST Starred"
                    icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kFolder];
}

+ (instancetype)recentItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItem alloc] initWithIdentifier:@"recent"
                                         title:@"TEST Recent"
                                          icon:DefaultSymbolWithPointSize(
                                                   kClockSymbol, kIconPointSize)
                                  creationDate:nil
                                          type:DriveItemType::kFolder];
}

+ (instancetype)sharedWithMeItem {
  // TODO(crbug.com/344812548): Add a11y title.
  return [[DriveItem alloc]
      initWithIdentifier:@"shared_with_me"
                   title:@"TEST Shared With Me"
                    icon:DefaultSymbolWithPointSize(kPersonTwoSymbol,
                                                    kIconPointSize)
            creationDate:nil
                    type:DriveItemType::kFolder];
}

@end
