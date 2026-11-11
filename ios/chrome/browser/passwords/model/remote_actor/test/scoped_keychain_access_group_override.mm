// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/test/scoped_keychain_access_group_override.h"

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"

ScopedRemoteActorKeychainAccessGroupOverrideForTesting::
    ScopedRemoteActorKeychainAccessGroupOverrideForTesting(
        NSString* access_group_override) {
  SetRemoteActorKeychainAccessGroupOverrideForTesting(  // IN-TEST
      access_group_override);
}

ScopedRemoteActorKeychainAccessGroupOverrideForTesting::
    ~ScopedRemoteActorKeychainAccessGroupOverrideForTesting() {
  ResetRemoteActorKeychainAccessGroupOverrideForTesting();  // IN-TEST
}
