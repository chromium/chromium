// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_FACTORY_H_

#import <Foundation/Foundation.h>

class WebStateList;
@class SessionWindowIOS;

// A factory that is used to create a SessionWindowIOS object for a specific
// WebStateList. It's the responsibility of the owner of the returned object
// to maintain a valid WebStateList and disconnect this object when the
// WebStateList is destroyed before this object.
@interface SessionWindowIOSFactory : NSObject

// Initialize the factory with a valid WebStateList.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Disconnect the object from any C++ object it may be referencing.
- (void)disconnect;

// Creates a SessionWindowIOS object with a serialized WebStateList. Returns
// nil if called after -disconnect.
- (SessionWindowIOS*)sessionForSaving;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_WINDOW_IOS_FACTORY_H_
