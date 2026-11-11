// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_STARTUP_TEST_UTIL_H_
#define IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_STARTUP_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/actor_request.h"

// Holds a generated Remote Actor URL and its expected parsed request.
struct RemoteActorTestData {
  NSURL* url;
  RemoteActorRequest expectation;
};

// Stores a random symmetric key in the Keychain and returns a valid Remote
// Actor URL with `scheme` along with the expected `RemoteActorRequest`.
RemoteActorTestData CreateRemoteActorTestData(NSString* scheme);

// Cleans up any symmetric key stored in the Keychain by
// `CreateRemoteActorTestData()`.
void CleanupRemoteActorTestData();

#endif  // IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_STARTUP_TEST_UTIL_H_
