// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_SCOPED_KEYCHAIN_ACCESS_GROUP_OVERRIDE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_SCOPED_KEYCHAIN_ACCESS_GROUP_OVERRIDE_H_

#import <Foundation/Foundation.h>

// RAII helper to temporarily override the Keychain access group during a test.
class ScopedRemoteActorKeychainAccessGroupOverrideForTesting {
 public:
  explicit ScopedRemoteActorKeychainAccessGroupOverrideForTesting(
      NSString* access_group_override);
  ~ScopedRemoteActorKeychainAccessGroupOverrideForTesting();
  ScopedRemoteActorKeychainAccessGroupOverrideForTesting(
      const ScopedRemoteActorKeychainAccessGroupOverrideForTesting&) = delete;
  ScopedRemoteActorKeychainAccessGroupOverrideForTesting& operator=(
      const ScopedRemoteActorKeychainAccessGroupOverrideForTesting&) = delete;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_SCOPED_KEYCHAIN_ACCESS_GROUP_OVERRIDE_H_
