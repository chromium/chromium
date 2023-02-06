// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_INTERACTION_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_INTERACTION_H_

#import <Foundation/Foundation.h>

@class UIFindInteraction;
@protocol CRWFindSession;

// Find interaction protocol to provide an abstract interface to
// UIFindInteraction and allow for fakes in tests.
API_AVAILABLE(ios(16))
@protocol CRWFindInteraction

// Whether the Find navigator is currently visible.
@property(nonatomic, readonly, getter=isFindNavigatorVisible)
    BOOL findNavigatorVisible;

// Current Find session, if any.
@property(nonatomic, readonly) id<CRWFindSession> activeFindSession;

// Can be set to prepopulate the Find navigator before it is presented.
@property(nonatomic, copy) NSString* searchText;

// Present the Find navigator. `showingReplace` is ignored.
- (void)presentFindNavigatorShowingReplace:(BOOL)showingReplace;

// Dismiss the Find navigator.
- (void)dismissFindNavigator;

@end

// Wrapper around UIFindInteraction which conforms to the CRWFindInteraction
// protocol and forward all calls to the underlying object.
API_AVAILABLE(ios(16))
@interface CRWFindInteraction : NSObject <CRWFindInteraction>

- (instancetype)init NS_UNAVAILABLE;

// Wraps the given `UIFindInteraction`. `UIFindInteraction` cannot be nil.
- (instancetype)initWithUIFindInteraction:(UIFindInteraction*)UIFindInteraction
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_CRW_FIND_INTERACTION_H_
