// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

#import "base/check.h"
#import "base/unguessable_token.h"

@implementation ComposeboxInputItem {
  base::UnguessableToken _identifier;
}

- (instancetype)initWithComposeboxInputItemType:(ComposeboxInputItemType)type
                                        assetID:(NSString*)assetID {
  self = [super init];
  if (self) {
    _identifier = base::UnguessableToken::Create();
    _state = ComposeboxInputItemState::kLoading;
    _type = type;
    _assetID = [assetID copy];
  }
  return self;
}

- (instancetype)initWithComposeboxInputItemType:(ComposeboxInputItemType)type {
  return [self initWithComposeboxInputItemType:type assetID:nil];
}

- (const base::UnguessableToken&)identifier {
  return _identifier;
}

- (BOOL)isEqual:(id)other {
  if (self == other) {
    return YES;
  }
  if (![other isKindOfClass:[ComposeboxInputItem class]]) {
    return NO;
  }
  ComposeboxInputItem* otherItem = (ComposeboxInputItem*)other;
  return _identifier == otherItem->_identifier;
}

- (NSUInteger)hash {
  return base::UnguessableTokenHash()(_identifier);
}

- (id)copyWithZone:(NSZone*)zone {
  ComposeboxInputItem* copy = [[ComposeboxInputItem allocWithZone:zone] init];
  if (copy) {
    // This is a shallow copy, but it's all that's needed for the diffable
    // data source. The UnguessableToken is copied by value.
    copy->_identifier = _identifier;
    copy.previewImage = self.previewImage;
    copy.title = self.title;
    copy.state = self.state;
    copy.type = self.type;
  }
  return copy;
}

@end
