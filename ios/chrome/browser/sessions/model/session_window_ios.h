// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_H_

#import <Foundation/Foundation.h>

@class CRWSessionStorage;
@class SessionTabGroup;

// Encapsulates everything required to save a session "window".
@interface SessionWindowIOS : NSObject<NSCoding>

// Initializes SessionsWindowIOS using the parameters are initial values for
// the `sessions` and `selectedIndex` properties. `selectedIndex` must be a
// valid index in `sessions` or NSNotFound if `sessions` is empty.
- (instancetype)initWithSessions:(NSArray<CRWSessionStorage*>*)sessions
                       tabGroups:(NSArray<SessionTabGroup*>*)tabGroups
                   selectedIndex:(NSUInteger)selectedIndex
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The serialized session objects. May be empty but never nil.
@property(nonatomic, readonly) NSArray<CRWSessionStorage*>* sessions;

// The serialized tab group objects. May be empty but never nil.
@property(nonatomic, readonly) NSArray<SessionTabGroup*>* tabGroups;

// The currently selected session. NSNotFound if the sessionWindow contains
// no sessions; otherwise a valid index in `sessions`.
@property(nonatomic, readonly) NSUInteger selectedIndex;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_H_
