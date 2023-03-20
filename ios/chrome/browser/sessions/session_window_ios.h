// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_

#import <Foundation/Foundation.h>

@class CRWSessionStorage;

// Encapsulates everything required to save a session "window".
@interface SessionWindowIOS : NSObject<NSCoding>

// Initializes SessionsWindowIOS using the parameters are initial values for
// the `sessions` and `selectedIndex` properties. `selectedIndex` must be a
// valid indice in `sessions` or NSNotFound if `sessions` is empty.
- (instancetype)initWithSessions:(NSArray<CRWSessionStorage*>*)sessions
                   selectedIndex:(NSUInteger)selectedIndex
    NS_DESIGNATED_INITIALIZER;

// The serialized session objects. May be empty but never nil.
@property(nonatomic, readonly) NSArray<CRWSessionStorage*>* sessions;

// The currently selected session. NSNotFound if the sessionWindow contains
// no sessions; otherwise a valid index in `sessions`.
@property(nonatomic, readonly) NSUInteger selectedIndex;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_
