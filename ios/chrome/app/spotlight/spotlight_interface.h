// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_INTERFACE_H_
#define IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_INTERFACE_H_

#import <UIKit/UIKit.h>

@class SpotlightLogger;
@class CSSearchableIndex;
@class CSSearchableItem;

/// An interface for CoreSpotlight that calls CoreSpotlight internally.
/// Use this API instead of calling CoreSpotlight directly.
/// It is designed to be called on the main thread, and will dispatch all the
/// callbacks on the main thread. It takes care of logging and some error
/// handling. It can also be stubbed in tests.
@interface SpotlightInterface : NSObject

// Interface backed by defaultSearchableIndex and the shared SpotlightLogger (if
// it is available).
+ (SpotlightInterface*)defaultInterface;

- (instancetype)initWithSearchableIndex:(CSSearchableIndex*)searchableIndex
                                 logger:(SpotlightLogger*)logger
                            maxAttempts:(NSUInteger)maxAttempts
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Searchable index used internally.
@property(nonatomic, readonly) CSSearchableIndex* searchableIndex;

// Logger used by this instance.
@property(nonatomic, readonly) SpotlightLogger* logger;

/// Adds or updates searchable items.
/// Takes care of retrying the call internally. No need to retry again, any
/// error is considered final (only the last error is reported in callback and
/// logged after some retry attempts). No need to log the error with
/// SpotlightLogger either, it's done internally. CompletionHandler will be
/// called on the main thread.
- (void)indexSearchableItems:(NSArray<CSSearchableItem*>*)items
           completionHandler:(void (^)(NSError* error))completionHandler;

/// Calls CSSearchableIndex API with the same name.
/// Takes care of retrying the call internally. No need to retry again, any
/// error is considered final (only the last error is reported in callback and
/// logged after some retry attempts). No need to log the error with
/// SpotlightLogger either, it's done internally. CompletionHandler will be
/// called on the main thread.
- (void)deleteSearchableItemsWithIdentifiers:(NSArray<NSString*>*)identifiers
                           completionHandler:
                               (void (^)(NSError* error))completionHandler;

/// Calls CSSearchableIndex API with the same name.
/// Takes care of retrying the call internally. No need to retry again, any
/// error is considered final (only the last error is reported in callback and
/// logged after some retry attempts). Domain identifiers should conform to
/// spotlight::StringFromSpotlightDomain. No need to log the error with
/// SpotlightLogger either, it's done internally.
/// CompletionHandler will be called on the main thread.
- (void)deleteSearchableItemsWithDomainIdentifiers:
            (NSArray<NSString*>*)domainIdentifiers
                                 completionHandler:(void (^)(NSError* error))
                                                       completionHandler;

/// Calls CSSearchableIndex API with the same name.
/// Takes care of retrying the call internally. No need to retry again, any
/// error is considered final (only the last error is reported in callback and
/// logged after some retry attempts). No need to log the error with
/// SpotlightLogger either, it's done internally. CompletionHandler will be
/// called on the main thread.
- (void)deleteAllSearchableItemsWithCompletionHandler:
    (void (^)(NSError* error))completionHandler;

@end
#endif  // IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_INTERFACE_H_
