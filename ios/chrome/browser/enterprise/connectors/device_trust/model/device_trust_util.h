// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_UTIL_H_

class ProfileIOS;

// Returns true if `DeviceTrustJavaScriptFeature` and
// `DeviceTrustChallengeTabHelper` should be registered for `profile`: the
// feature flag is enabled and `profile` is a regular (non-off-the-record)
// profile. `profile` must not be null.
//
// Registration is intentionally not gated on management state, which can
// change mid-session; `IsEnabled()` and `Watches()` dynamically gate API setup
// and attestation requests instead.
// TODO(crbug.com/517112324): Revisit management gating when registration can
// react to management changes without requiring an app restart.
bool ShouldRegisterDeviceTrust(const ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_UTIL_H_
