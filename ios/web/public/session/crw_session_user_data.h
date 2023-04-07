// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_SESSION_USER_DATA_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_SESSION_USER_DATA_H_

#import <Foundation/Foundation.h>

// CRWSessionUserData serializes a mapping of key=value that corresponds
// to the user data attached to a WebState (via SerializableUserDataManager).
//
// This class is used as its interface can be kept private to //ios/web while
// being forward-declared in a public header. Code outside of //ios/web cannot
// create instances and thus cannot break the invariant of the session saving
// code.
@interface CRWSessionUserData : NSObject <NSCoding>

// Adds a mapping from `key` to `object`.
- (void)setObject:(id<NSCoding>)object forKey:(NSString*)key;

// Gets the object mapped to `key` or nil if not present.
- (id<NSCoding>)objectForKey:(NSString*)key;

// Removes the mapping for `key`.
- (void)removeObjectForKey:(NSString*)key;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_SESSION_USER_DATA_H_
