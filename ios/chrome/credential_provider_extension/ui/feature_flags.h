// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_

#import <Foundation/Foundation.h>

#import <optional>

// Whether automatic passkey upgrade is enabled for the user.
BOOL IsAutomaticPasskeyUpgradeEnabled();

// Whether passkey PRF support is enabled.
BOOL IsPasskeyPRFEnabled();

// Whether password creation is enabled for this user by preference.
BOOL IsPasswordCreationUserEnabled();

// Whether password creation enabled/disabled state is controlled by an
// enterprise policy.
BOOL IsPasswordCreationManaged();

// Whether password sync is enabled for this user.
BOOL IsPasswordSyncEnabled();

// Policy about whether passkey saving is allowed. The return value isn't set if
// the policy isn't set in user defaults. The policy itself returns `true` for
// unmanaged users, or for users whose enterprise has not configured this
// policy. IMPORTANT: If `IsPasswordCreationUserEnabled()` is `NO`, that
// supersedes this policy.
std::optional<bool> GetPasskeyCreationPolicy();

// Whether the passkeys M2 feature is currently enabled.
BOOL IsPasskeysM2Enabled();

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
