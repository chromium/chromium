// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_interface.h"

#import "base/check.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SpotlightInterface ()

// Maximum retry attempts to delete/index a set of items.
@property(nonatomic, assign) NSUInteger maxAttempts;

@end

@implementation SpotlightInterface

+ (SpotlightInterface*)defaultInterface {
  static SpotlightInterface* const kDefaultSpotlightInterface =
      [[SpotlightInterface alloc]
          initWithSearchableIndex:[CSSearchableIndex defaultSearchableIndex]
                      maxAttempts:spotlight::kMaxAttempts - 1];
  return kDefaultSpotlightInterface;
}

// Execute blockToRetry with up to retryCount retries on error. Execute
// callback when done.
+ (void)doWithRetry:(void (^)(BlockWithError error))blockToRetry
           retryCount:(NSUInteger)retryCount
    completionHandler:(BlockWithError)completionHandler {
  DCHECK(completionHandler);

  blockToRetry(^(NSError* error) {
    if (error && retryCount > 0) {
      [SpotlightInterface doWithRetry:blockToRetry
                           retryCount:retryCount - 1
                    completionHandler:completionHandler];
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        completionHandler(error);
      });
    }
  });
}

- (instancetype)initWithSearchableIndex:(CSSearchableIndex*)searchableIndex
                            maxAttempts:(NSUInteger)maxAttempts {
  self = [super init];
  if (self) {
    _searchableIndex = searchableIndex;
    _maxAttempts = maxAttempts;
  }
  return self;
}

- (void)indexSearchableItems:(NSArray<CSSearchableItem*>*)items {
  __weak SpotlightInterface* weakSelf = self;

  void (^addItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [weakSelf.searchableIndex indexSearchableItems:items
                                 completionHandler:errorBlock];
  };

  [SpotlightInterface doWithRetry:addItems
                       retryCount:self.maxAttempts
                completionHandler:^(NSError* error) {
                  [SpotlightLogger logSpotlightError:error];
                }];
}

- (void)deleteSearchableItemsWithIdentifiers:(NSArray<NSString*>*)identifiers
                           completionHandler:(BlockWithError)completionHandler {
  __weak SpotlightInterface* weakSelf = self;

  BlockWithError augmentedCallback = ^(NSError* error) {
    [SpotlightLogger logSpotlightError:error];

    if (completionHandler) {
      completionHandler(error);
    }
  };

  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [weakSelf.searchableIndex deleteSearchableItemsWithIdentifiers:identifiers
                                                 completionHandler:errorBlock];
  };

  [SpotlightInterface doWithRetry:deleteItems
                       retryCount:self.maxAttempts
                completionHandler:augmentedCallback];
}

- (void)deleteSearchableItemsWithDomainIdentifiers:
            (NSArray<NSString*>*)domainIdentifiers
                                 completionHandler:
                                     (BlockWithError)completionHandler {
  __weak SpotlightInterface* weakSelf = self;

  BlockWithError augmentedCallback = ^(NSError* error) {
    [SpotlightLogger logSpotlightError:error];

    if (completionHandler) {
      completionHandler(error);
    }
  };

  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [weakSelf.searchableIndex
        deleteSearchableItemsWithDomainIdentifiers:domainIdentifiers
                                 completionHandler:errorBlock];
  };

  [SpotlightInterface doWithRetry:deleteItems
                       retryCount:self.maxAttempts
                completionHandler:augmentedCallback];
}

- (void)deleteAllSearchableItemsWithCompletionHandler:
    (BlockWithError)completionHandler {
  __weak SpotlightInterface* weakSelf = self;

  BlockWithError augmentedCallback = ^(NSError* error) {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:@(spotlight::kSpotlightLastIndexingDateKey)];

    [SpotlightLogger logSpotlightError:error];

    if (completionHandler) {
      completionHandler(error);
    }
  };

  void (^deleteItems)(BlockWithError) = ^(BlockWithError errorBlock) {
    [weakSelf.searchableIndex
        deleteAllSearchableItemsWithCompletionHandler:errorBlock];
  };

  [SpotlightInterface doWithRetry:deleteItems
                       retryCount:self.maxAttempts
                completionHandler:augmentedCallback];
}

@end
