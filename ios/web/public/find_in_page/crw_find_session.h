// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_SESSION_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_SESSION_H_

#import <UIKit/UIKit.h>

// Find session protocol to provide an abstract interface to UIFindSession and
// allow for fakes in tests.
API_AVAILABLE(ios(16))
@protocol CRWFindSession

// The number of results for the last performed query.
@property(nonatomic, readonly) NSInteger resultCount;

// The currently highlighted result index.
@property(nonatomic, readonly) NSInteger highlightedResultIndex;

// Performs a text search with the given `query` and `options`.
- (void)performSearchWithQuery:(NSString*)query
                       options:(UITextSearchOptions*)options;

// Selects next or previous result depending on `direction`.
- (void)highlightNextResultInDirection:(UITextStorageDirection)direction;

// Resets the Find session data.
- (void)invalidateFoundResults;

@end

// Wrapper around UIFindSession which conforms to the CRWFindSession protocol
// and forward all calls to the underlying object.
API_AVAILABLE(ios(16))
@interface CRWFindSession : NSObject <CRWFindSession>

- (instancetype)init NS_UNAVAILABLE;

// Wraps the given `UIFindSession`. `UIFindSession` cannot be nil.
- (instancetype)initWithUIFindSession:(UIFindSession*)UIFindSession
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_SESSION_H_
