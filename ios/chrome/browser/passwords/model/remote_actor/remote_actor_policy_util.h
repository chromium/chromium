// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_POLICY_UTIL_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_POLICY_UTIL_H_

class ProfileIOS;

// Returns whether enterprise policy permits Remote Actor credential sharing
// (`GeminiSparkSettings`) for `profile`.
//
// Remote Actor credential sharing is disabled by default for enterprise-managed
// users (managed browser/profile, managed device, enterprise primary account,
// or policy-managed Gemini prefs). For managed profiles, sharing is allowed
// only when `GeminiSparkSettings` is explicitly set to Enabled (`0`) and
// prerequisite Gemini / Act-on-Web policies are not disabled (dogfood clients
// are exempted, matching Desktop Glic policy behavior).
//
// For unmanaged non-incognito profiles, returns `true`. Returns `false` if
// `profile` is null or off-the-record.
bool IsRemoteActorCredentialSharingAllowedByPolicy(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_POLICY_UTIL_H_
