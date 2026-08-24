// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SWITCHES_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SWITCHES_H_

class GURL;

namespace webauthn {

inline constexpr char kDeviceAuthorizationKeyEndpointUrl[] =
    "https://chromesyncpasswords-pa.googleapis.com/v1/users/me/"
    "deviceAuthorizationKey";

inline constexpr char kDeviceAuthorizationKeyEndpointUrlSwitch[] =
    "device-authorization-key-server-url";

// Returns the endpoint URL for device authorization key requests. If the
// `--device-authorization-key-server-url` command line switch is set and
// contains a valid URL, it is returned instead of the default endpoint.
GURL GetDeviceAuthorizationKeyEndpointUrl();

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_DEVICE_AUTHORIZATION_DEVICE_AUTHORIZATION_SWITCHES_H_
