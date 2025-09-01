// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/shared/ui/util/date_localized_util.h"

@implementation DownloadListGroupItem

- (instancetype)initWithItems:(NSArray<DownloadListItem*>*)items
                localMidnight:(base::Time)localMidnight {
  self = [super init];
  if (self) {
    CHECK(!localMidnight.is_null());
    _items = [items copy];
    _localMidnight = localMidnight;
  }
  return self;
}

- (NSString*)title {
  std::u16string dateString =
      date_localized::GetRelativeDateLocalized(_localMidnight);
  return base::SysUTF16ToNSString(dateString);
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[DownloadListGroupItem class]]) {
    return NO;
  }
  DownloadListGroupItem* other = object;
  return _localMidnight == other.localMidnight;
}

- (NSUInteger)hash {
  base::TimeDelta delta = _localMidnight - base::Time::UnixEpoch();
  return static_cast<NSUInteger>(delta.InSeconds());
}

@end
