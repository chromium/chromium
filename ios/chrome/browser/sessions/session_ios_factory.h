// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

class WebStateList;
@class SessionIOS;

// A factory that is used to create a SessionIOS object for a specific
// WebStateList. It's the responsibility of the owner of the SessionIOSFactory
// object to maintain a valid WebStateList and disconnect this object if the
// webStateList is destroyed before this object.
@interface SessionIOSFactory : NSObject

// Initialize the factory with a valid WebStateList.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Disconnect the object by clearing the internal webStateList.
- (void)disconnect;

// Creates a sessionIOS object with a serialized webStateList. This method can't
// be used without initializing the object with a non-null WebStateList.
- (SessionIOS*)sessionForSaving;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_IOS_FACTORY_H_
