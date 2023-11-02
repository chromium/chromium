// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_REMOTING_SERVICE_H_
#define REMOTING_IOS_FACADE_REMOTING_SERVICE_H_

#import "remoting/client/chromoting_client_runtime.h"

@class UserInfo;

@protocol RemotingAuthentication;

// Eventing related keys:

// User did update event name.
extern NSString* const kUserDidUpdate;
// Map key for UserInfo object.
extern NSString* const kUserInfo;

// |RemotingService| is the centralized place to ask for information about
// authentication or query the remote services. It also helps deal with the
// runtime and threading used in the application. |RemotingService| is a
// singleton and should only be accessed via the instance property.
@interface RemotingService : NSObject

// Access to the singleton shared instance from this property.
@property(nonatomic, readonly, class) RemotingService* instance;

// The Chromoting Client Runtime, this holds the threads and other shared
// resources used by the Chromoting clients
@property(nonatomic, readonly) remoting::ChromotingClientRuntime* runtime;

// TODO(yuweih): Make |authentication| its own singleton.
// This must be set immediately after the authentication object is created. It
// can only be set once.
@property(nonatomic) id<RemotingAuthentication> authentication;

@end

#endif  // REMOTING_IOS_FACADE_REMOTING_SERVICE_H_
