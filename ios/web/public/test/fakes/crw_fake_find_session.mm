// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_find_session.h"

@implementation CRWFakeFindSession

@synthesize highlightedResultIndex = _highlightedResultIndex;
@synthesize resultCount = _resultCount;

- (id)copyWithZone:(NSZone*)zone {
  CRWFakeFindSession* copy = [[CRWFakeFindSession alloc] init];
  copy.resultCountsForQueries = _resultCountsForQueries;
  return copy;
}

- (void)performSearchWithQuery:(NSString*)query
                       options:(UITextSearchOptions*)options {
  _resultCount = _resultCountsForQueries[query].intValue;
  // Starting with no highlighted result.
  _highlightedResultIndex = NSNotFound;
}

- (void)highlightNextResultInDirection:(UITextStorageDirection)direction {
  if (_resultCount == 0) {
    return;
  }

  if (_highlightedResultIndex == NSNotFound) {
    switch (direction) {
      case UITextStorageDirectionForward:
        _highlightedResultIndex = 0;
        break;

      case UITextStorageDirectionBackward:
        _highlightedResultIndex = _resultCount - 1;
        break;
    }

    return;
  }

  switch (direction) {
    case UITextStorageDirectionForward:
      _highlightedResultIndex = (_highlightedResultIndex + 1) % _resultCount;
      break;

    case UITextStorageDirectionBackward:
      _highlightedResultIndex =
          (_highlightedResultIndex + _resultCount - 1) % _resultCount;
      break;
  }
}

- (void)invalidateFoundResults {
  _resultCount = 0;
  _highlightedResultIndex = NSNotFound;
}

@end
