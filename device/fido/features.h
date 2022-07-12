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

// Support the caBLE extension in assertion requests from any origin.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableExtensionAnywhere;

// Enable discoverable credentials on caBLE authenticators.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCableDisco;

#if BUILDFLAG(IS_CHROMEOS)
// Enable a ChromeOS platform authenticator
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCrosPlatformAuthenticator;
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kU2fPermissionPrompt;

// Feature flag for the Google-internal
// `WebAuthenticationAllowGoogleCorpRemoteRequestProxying` enterprise policy.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthnGoogleCorpRemoteDesktopClientPrivilege;

// Enable some experimental UI changes
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthPasskeysUI;

// Reshuffle WebAuthn request UI to put account selection for discoverable
// credentials on platform authenticators first, where applicable.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthnNewDiscoverableCredentialsUi;

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
