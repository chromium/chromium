// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/crw_find_session.h"

#import "base/check.h"

@implementation CRWFindSession {
  UIFindSession* _UIFindSession;
}

- (instancetype)initWithUIFindSession:(UIFindSession*)UIFindSession {
  DCHECK(UIFindSession);
  self = [super init];
  if (self) {
    _UIFindSession = UIFindSession;
  }
  return self;
}

- (NSInteger)resultCount {
  return [_UIFindSession resultCount];
}

- (NSInteger)highlightedResultIndex {
  return [_UIFindSession highlightedResultIndex];
}

- (void)performSearchWithQuery:(NSString*)query
                       options:(UITextSearchOptions*)options {
  [_UIFindSession performSearchWithQuery:query options:options];
}

- (void)highlightNextResultInDirection:(UITextStorageDirection)direction {
  [_UIFindSession highlightNextResultInDirection:direction];
}

- (void)invalidateFoundResults {
  [_UIFindSession invalidateFoundResults];
}

@end
