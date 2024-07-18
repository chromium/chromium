// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_tab_group.h"

#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_id.h"

using tab_groups::TabGroupId;

namespace {

// Keys used to serialize properties.
NSString* const kRangeStartKey = @"kRangeStartKey";
NSString* const kRangeCountKey = @"kRangeCountKey";
NSString* const kTitleKey = @"kTitleKey";
NSString* const kColorIdKey = @"kColorIdKey";
NSString* const kcollapsedStateKey = @"kcollapsedStateKey";
NSString* const kTabGroupIdKey = @"kTabGroupIdKey";

}  // namespace

@implementation SessionTabGroup {
  std::optional<TabGroupId> tabGroupId_;
}

- (instancetype)initWithRangeStart:(NSInteger)rangeStart
                        rangeCount:(NSInteger)rangeCount
                             title:(NSString*)title
                           colorId:(NSInteger)colorId
                    collapsedState:(BOOL)collapsedState
                        tabGroupId:(TabGroupId)tabGroupId {
  self = [super init];
  if (self) {
    _rangeStart = rangeStart;
    _rangeCount = rangeCount;
    _title = title ?: @"";
    _colorId = colorId;
    _collapsedState = collapsedState;
    tabGroupId_ = tabGroupId;
  }
  return self;
}

#pragma mark - Getters

- (TabGroupId)tabGroupId {
  if (!tabGroupId_.has_value()) {
    tabGroupId_ = TabGroupId::CreateEmpty();
  }
  return tabGroupId_.value();
}

#pragma mark - NSCoding

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeInt:_rangeStart forKey:kRangeStartKey];
  [coder encodeInt:_rangeCount forKey:kRangeCountKey];
  [coder encodeObject:_title forKey:kTitleKey];
  [coder encodeInt:_colorId forKey:kColorIdKey];
  [coder encodeBool:_collapsedState forKey:kcollapsedStateKey];
  [coder
      encodeObject:base::SysUTF8ToNSString(self.tabGroupId.token().ToString())
            forKey:kTabGroupIdKey];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return [self initWithRangeStart:[coder decodeIntForKey:kRangeStartKey]
                       rangeCount:[coder decodeIntForKey:kRangeCountKey]
                            title:[coder decodeObjectForKey:kTitleKey]
                          colorId:[coder decodeIntForKey:kColorIdKey]
                   collapsedState:[coder decodeBoolForKey:kcollapsedStateKey]
                       tabGroupId:[self decodeTabGroupId:coder]];
}

#pragma mark - Private

// Decodes the tabGroupId object.
- (TabGroupId)decodeTabGroupId:(NSCoder*)coder {
  if (NSString* tabGroupIdString = [coder decodeObjectForKey:kTabGroupIdKey]) {
    std::optional<base::Token> token =
        base::Token::FromString(base::SysNSStringToUTF8(tabGroupIdString));
    if (token.has_value()) {
      return TabGroupId::FromRawToken(*token);
    }
  }
  return TabGroupId::GenerateNew();
}

@end
