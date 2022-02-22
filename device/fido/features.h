// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace url {
class Origin;
}

namespace device {

#if BUILDFLAG(IS_WIN)
// Controls whether on Windows, U2F/CTAP2 requests are forwarded to the
// native WebAuthentication API, where available.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthUseNativeWinApi;
#endif  // BUILDFLAG(IS_WIN)

// Enable synced Android devices to be a 2nd-factor security key.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableSecondFactor;

// Enable using a phone as a generic security key.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthPhoneSupport;

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableExtensionAnywhere;

// Enable discoverable credentials on caBLE authenticators.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableDisco;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enable a ChromeOS platform authenticator
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCrosPlatformAuthenticator;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kU2fPermissionPrompt;

// Feature flag for the Google-internal
// `WebAuthenticationAllowGoogleCorpRemoteRequestProxying` enterprise policy.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthnGoogleCorpRemoteDesktopClientPrivilege;

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
