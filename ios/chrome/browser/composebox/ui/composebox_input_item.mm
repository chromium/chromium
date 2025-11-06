// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

#import "base/check.h"
#import "base/unguessable_token.h"

@implementation ComposeboxInputItem {
  base::UnguessableToken _token;
}

- (instancetype)initWithComposeboxInputItemType:(ComposeboxInputItemType)type {
  self = [super init];
  if (self) {
    _token = base::UnguessableToken::Create();
    _state = ComposeboxInputItemState::kLoading;
    _type = type;
  }
  return self;
}

- (const base::UnguessableToken&)token {
  return _token;
}

- (BOOL)isEqual:(id)other {
  if (self == other) {
    return YES;
  }
  if (![other isKindOfClass:[ComposeboxInputItem class]]) {
    return NO;
  }
  ComposeboxInputItem* otherItem = (ComposeboxInputItem*)other;
  return _token == otherItem->_token;
}

- (NSUInteger)hash {
  return base::UnguessableTokenHash()(_token);
}

- (id)copyWithZone:(NSZone*)zone {
  ComposeboxInputItem* copy = [[ComposeboxInputItem allocWithZone:zone] init];
  if (copy) {
    // This is a shallow copy, but it's all that's needed for the diffable
    // data source. The UnguessableToken is copied by value.
    copy->_token = _token;
    copy.previewImage = self.previewImage;
    copy.title = self.title;
    copy.state = self.state;
    copy.type = self.type;
  }
  return copy;
}

@end
