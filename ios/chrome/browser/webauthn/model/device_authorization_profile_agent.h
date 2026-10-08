// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_DEVICE_AUTHORIZATION_PROFILE_AGENT_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_DEVICE_AUTHORIZATION_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "ios/chrome/app/profile/observing_profile_agent.h"

// Name of the deferred block fetching the keys.
extern NSString* const kDeviceAuthorizationFetchBlockName;

// Backoff after failed fetches, doubled after each failure up to the maximum.
inline constexpr base::TimeDelta kDeviceAuthorizationInitialBackoff =
    base::Hours(1);
inline constexpr base::TimeDelta kDeviceAuthorizationMaximumBackoff =
    base::Days(7);

// Silently fetches device authorization keys for the primary account, so that
// they are available locally when needed for passkey operations.
//
// When the profile reaches `ProfileInitStage::kFinal` and whenever a
// new primary account is set, it checks whether valid keys are cached and
// fetches them if not. If UI is required for the fetch, it will not be handled
// here. Fetch attempts back off exponentially until valid keys are cached.
// The backoff restarts whenever the primary account changes. The logic is
// handled in a deferred block to avoid competing with startup work.
@interface DeviceAuthorizationProfileAgent : ObservingProfileAgent
@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_DEVICE_AUTHORIZATION_PROFILE_AGENT_H_
