/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"

@implementation SafariDataItem

@synthesize type = _type;
@synthesize status = _status;

- (instancetype)initWithType:(SafariDataItemType)type {
  self = [super init];
  if (self) {
    _type = type;
    _status = SafariDataItemImportStatus::kReady;
  }
  return self;
}

@end
