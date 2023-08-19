// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/fake_spotlight_interface.h"

#import "ios/chrome/app/spotlight/spotlight_util.h"

@implementation FakeSpotlightInterface

- (FakeSpotlightInterface*)init {
  self = [super init];

  if (self) {
    _indexSearchableItemsCallsCount = 0;
    _deleteSearchableItemsWithIdentifiersCallsCount = 0;
    _deleteSearchableItemsWithDomainIdentifiersCallsCount = 0;
    _deleteAllSearchableItemsWithCompletionHandlerCallsCount = 0;
  }

  return self;
}

- (void)indexSearchableItems:(NSArray<CSSearchableItem*>*)items {
  _indexSearchableItemsCallsCount++;
}

- (void)deleteSearchableItemsWithIdentifiers:(NSArray<NSString*>*)identifiers
                           completionHandler:(BlockWithError)completionHandler {
  _deleteSearchableItemsWithIdentifiersCallsCount++;
  if (completionHandler) {
    completionHandler(nil);
  }
}

- (void)deleteSearchableItemsWithDomainIdentifiers:
            (NSArray<NSString*>*)domainIdentifiers
                                 completionHandler:
                                     (BlockWithError)completionHandler {
  _deleteSearchableItemsWithDomainIdentifiersCallsCount++;
  if (completionHandler) {
    completionHandler(nil);
  }
}

- (void)deleteAllSearchableItemsWithCompletionHandler:
    (BlockWithError)completionHandler {
  _deleteAllSearchableItemsWithCompletionHandlerCallsCount++;
  if (completionHandler) {
    completionHandler(nil);
  }
}

@end
