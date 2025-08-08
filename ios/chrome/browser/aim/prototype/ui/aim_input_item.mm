// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"

#import "base/check.h"
#import "base/unguessable_token.h"

@implementation AIMInputItem {
  base::UnguessableToken _fileToken;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _fileToken = base::UnguessableToken::Create();
    _state = AIMInputItemState::kLoading;
  }
  return self;
}

- (const base::UnguessableToken&)fileToken {
  return _fileToken;
}

- (BOOL)isEqual:(id)other {
  if (self == other) {
    return YES;
  }
  if (![other isKindOfClass:[AIMInputItem class]]) {
    return NO;
  }
  AIMInputItem* otherItem = (AIMInputItem*)other;
  return _fileToken == otherItem->_fileToken;
}

- (NSUInteger)hash {
  return base::UnguessableTokenHash()(_fileToken);
}

- (id)copyWithZone:(NSZone*)zone {
  AIMInputItem* copy = [[AIMInputItem allocWithZone:zone] init];
  if (copy) {
    // This is a shallow copy, but it's all that's needed for the diffable
    // data source. The UnguessableToken is copied by value.
    copy->_fileToken = _fileToken;
    copy.previewImage = self.previewImage;
    copy.state = self.state;
  }
  return copy;
}

@end
