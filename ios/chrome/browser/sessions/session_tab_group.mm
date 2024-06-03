// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_tab_group.h"

namespace {

// Keys used to serialize properties.
NSString* const kRangeStartKey = @"kRangeStartKey";
NSString* const kRangeCountKey = @"kRangeCountKey";
NSString* const kTitleKey = @"kTitleKey";
NSString* const kColorIdKey = @"kColorIdKey";
NSString* const kcollapsedStateKey = @"kcollapsedStateKey";

}  // namespace

@implementation SessionTabGroup

- (instancetype)initWithRangeStart:(NSInteger)rangeStart
                        rangeCount:(NSInteger)rangeCount
                             title:(NSString*)title
                           colorId:(NSInteger)colorId
                    collapsedState:(BOOL)collapsedState {
  self = [super init];
  if (self) {
    _rangeStart = rangeStart;
    _rangeCount = rangeCount;
    _title = title ?: @"";
    _colorId = colorId;
    _collapsedState = collapsedState;
  }
  return self;
}

#pragma mark - NSCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeInt:_rangeStart forKey:kRangeStartKey];
  [coder encodeInt:_rangeCount forKey:kRangeCountKey];
  [coder encodeObject:_title forKey:kTitleKey];
  [coder encodeInt:_colorId forKey:kColorIdKey];
  [coder encodeBool:_collapsedState forKey:kcollapsedStateKey];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return [self initWithRangeStart:[coder decodeIntForKey:kRangeStartKey]
                       rangeCount:[coder decodeIntForKey:kRangeCountKey]
                            title:[coder decodeObjectForKey:kTitleKey]
                          colorId:[coder decodeIntForKey:kColorIdKey]
                   collapsedState:[coder decodeBoolForKey:kcollapsedStateKey]];
}

@end
