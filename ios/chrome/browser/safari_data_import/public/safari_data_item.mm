/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"

#import "base/check_op.h"
#import "base/notreached.h"

@implementation SafariDataItem

@synthesize type = _type;
@synthesize status = _status;
@synthesize count = _count;

- (instancetype)initWithType:(SafariDataItemType)type
                      status:(SafariDataItemImportStatus)status
                       count:(size_t)count {
  self = [super init];
  if (self) {
    _type = type;
    _status = status;
    _count = static_cast<int>(count);
  }
  return self;
}

- (void)transitionToNextStatus {
  switch (self.status) {
    case SafariDataItemImportStatus::kReady:
      _status = SafariDataItemImportStatus::kImporting;
      break;
    case SafariDataItemImportStatus::kImporting:
      _status = SafariDataItemImportStatus::kImported;
      break;
    case SafariDataItemImportStatus::kImported:
      NOTREACHED() << "item of type " << static_cast<NSUInteger>(self.type)
                   << " is already imported";
  }
}

@end
