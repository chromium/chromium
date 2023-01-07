// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_

#import <Foundation/Foundation.h>

@class CRWSessionStorage;

// Encapsulate minimum data about a tab.
// This data about each tab is always available, even if the data on disk is
// not deserialized.
// This is the only data available for unrealized webStates.
@interface SessionSummary : NSObject <NSCoding>
// The current URL of the session.
@property(nonatomic, readonly) NSURL* url;
// The current title of the session.
@property(nonatomic, readonly) NSString* title;
// The stable identifier of the session.
@property(nonatomic, readonly) NSString* stableIdentifier;

- (instancetype)initWithURL:(NSURL*)url
                      title:(NSString*)title
           stableIdentifier:(NSString*)stableIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Encapsulates everything required to save a session "window".
@interface SessionWindowIOS : NSObject<NSCoding>

// Initializes SessionsWindowIOS using the parameters are initial values for
// the `sessions` and `selectedIndex` properties. `selectedIndex` must be a
// valid indice in `sessions` or NSNotFound if `sessions` is empty.
- (instancetype)initWithSessions:(NSArray<CRWSessionStorage*>*)sessions
                 sessionsSummary:(NSArray<SessionSummary*>*)sessionsSummary
                     tabContents:(NSDictionary<NSString*, NSData*>*)tabContents
                   selectedIndex:(NSUInteger)selectedIndex
    NS_DESIGNATED_INITIALIZER;

// The serialized session objects. May be empty but never nil.
@property(nonatomic, readonly) NSArray<CRWSessionStorage*>* sessions;

// Contains basic data about each tab.
@property(nonatomic, readonly) NSArray<SessionSummary*>* sessionsSummary;

// Contains the serialized data for each tab.
@property(nonatomic, readonly) NSDictionary<NSString*, NSData*>* tabContents;

// The currently selected session. NSNotFound if the sessionWindow contains
// no sessions; otherwise a valid index in `sessions`.
@property(nonatomic, readonly) NSUInteger selectedIndex;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_IOS_H_
