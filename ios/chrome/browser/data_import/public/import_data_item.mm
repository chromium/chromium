/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/import_data_item.h"

#import "base/check_op.h"
#import "base/notreached.h"

@implementation ImportDataItem

@synthesize type = _type;
@synthesize status = _status;
@synthesize count = _count;

- (instancetype)initWithType:(ImportDataItemType)type
                      status:(ImportDataItemImportStatus)status
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
    case ImportDataItemImportStatus::kReady:
      _status = ImportDataItemImportStatus::kImporting;
      break;
    case ImportDataItemImportStatus::kImporting:
      _status = ImportDataItemImportStatus::kImported;
      break;
    case ImportDataItemImportStatus::kBlockedByPolicy:
    case ImportDataItemImportStatus::kImported:
      NOTREACHED() << "item of type " << static_cast<NSUInteger>(self.type)
                   << " is in a terminal state and cannot be transitioned";
  }
}

@end
