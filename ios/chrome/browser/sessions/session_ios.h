// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_H_

#import <Foundation/Foundation.h>

@class SessionWindowIOS;

// Encapsulates everything required to save a session. A session is a set of
// one or more session windows that share the same browser state.
@interface SessionIOS : NSObject<NSCoding>

- (instancetype)initWithWindows:(NSArray<SessionWindowIOS*>*)sessionWindows
    NS_DESIGNATED_INITIALIZER;

// The serialized SessionWindowIOS objects. May be empty but never nil.
@property(nonatomic, readonly) NSArray<SessionWindowIOS*>* sessionWindows;

// A dictionary containing the content of every tab in the session.
// The key is the WebState stable identifier.
// Only available if kSaveSessionTabsToSeparateFiles is enabled.
- (NSDictionary*)sessionTabContents;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_H_
