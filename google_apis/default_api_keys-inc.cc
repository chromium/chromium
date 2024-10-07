// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/default_api_keys.h"

// This file contains a definition of `GetDefaultApiKeysFromDefinedValues()`
// which transforms a bunch of preprocessor defines into a struct that can be
// supplied to the `ApiKeyCache` constructor.
//
// This file is intended to be included several times, specifically for tests
// that manipulate preprocessor defines rather than the struct members.
//
// Please keep this file's list of dependencies minimal.

#if !defined(GOOGLE_API_KEY)
#define GOOGLE_API_KEY google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_METRICS_SIGNING_KEY)
#define GOOGLE_METRICS_SIGNING_KEY google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_ID_MAIN)
#define GOOGLE_CLIENT_ID_MAIN google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_SECRET_MAIN)
#define GOOGLE_CLIENT_SECRET_MAIN google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_ID_REMOTING)
#define GOOGLE_CLIENT_ID_REMOTING google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_SECRET_REMOTING)
#define GOOGLE_CLIENT_SECRET_REMOTING \
  google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_ID_REMOTING_HOST)
#define GOOGLE_CLIENT_ID_REMOTING_HOST \
  google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_CLIENT_SECRET_REMOTING_HOST)
#define GOOGLE_CLIENT_SECRET_REMOTING_HOST \
  google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if BUILDFLAG(IS_ANDROID)
#if !defined(GOOGLE_API_KEY_ANDROID_NON_STABLE)
#define GOOGLE_API_KEY_ANDROID_NON_STABLE \
  google_apis::DefaultApiKeys::kUnsetApiToken
#endif
#endif

#if !defined(GOOGLE_API_KEY_REMOTING)
#define GOOGLE_API_KEY_REMOTING google_apis::DefaultApiKeys::kUnsetApiToken
#endif

// API key for the Speech On-Device API (SODA).
#if !defined(GOOGLE_API_KEY_SODA)
#define GOOGLE_API_KEY_SODA google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !BUILDFLAG(IS_ANDROID)
// API key for the HaTS API.
#if !defined(GOOGLE_API_KEY_HATS)
#define GOOGLE_API_KEY_HATS google_apis::DefaultApiKeys::kUnsetApiToken
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// API key for the Nearby Sharing Service.
#if !defined(GOOGLE_API_KEY_SHARING)
#define GOOGLE_API_KEY_SHARING google_apis::DefaultApiKeys::kUnsetApiToken
#endif

// API key for the ReadAloud API.
#if !defined(GOOGLE_API_KEY_READ_ALOUD)
#define GOOGLE_API_KEY_READ_ALOUD google_apis::DefaultApiKeys::kUnsetApiToken
#endif

// API key for the Fresnel API.
#if !defined(GOOGLE_API_KEY_FRESNEL)
#define GOOGLE_API_KEY_FRESNEL google_apis::DefaultApiKeys::kUnsetApiToken
#endif

#if !defined(GOOGLE_API_KEY_BOCA)
#define GOOGLE_API_KEY_BOCA google_apis::DefaultApiKeys::kUnsetApiToken
#endif
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// These are used as shortcuts for developers and users providing
// OAuth credentials via preprocessor defines or environment
// variables.  If set, they will be used to replace any of the client
// IDs and secrets above that have not been set (and only those; they
// will not override already-set values).
#if !defined(GOOGLE_DEFAULT_CLIENT_ID)
#define GOOGLE_DEFAULT_CLIENT_ID ""
#endif
#if !defined(GOOGLE_DEFAULT_CLIENT_SECRET)
#define GOOGLE_DEFAULT_CLIENT_SECRET ""
#endif

constexpr ::google_apis::DefaultApiKeys GetDefaultApiKeysFromDefinedValues() {
  return {
      // TODO(crbug.com/40214105): Rewrite this condition using
      // BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY).
      .allow_unset_values =
          !BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_FUCHSIA),
      .allow_override_via_environment = !BUILDFLAG(GOOGLE_CHROME_BRANDING),
      .google_api_key = GOOGLE_API_KEY,
      .google_metrics_signing_key = GOOGLE_METRICS_SIGNING_KEY,
#if BUILDFLAG(IS_ANDROID)
      .google_api_key_android_non_stable = GOOGLE_API_KEY_ANDROID_NON_STABLE,
#else
      .google_api_key_hats = GOOGLE_API_KEY_HATS,
#endif  // BUILDFLAG(IS_ANDROID)
      .google_api_key_remoting = GOOGLE_API_KEY_REMOTING,
      .google_api_key_soda = GOOGLE_API_KEY_SODA,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      .google_api_key_sharing = GOOGLE_API_KEY_SHARING,
      .google_api_key_read_aloud = GOOGLE_API_KEY_READ_ALOUD,
      .google_api_key_fresnel = GOOGLE_API_KEY_FRESNEL,
      .google_api_key_boca = GOOGLE_API_KEY_BOCA,
#endif
      .google_client_id_main = GOOGLE_CLIENT_ID_MAIN,
      .google_client_secret_main = GOOGLE_CLIENT_SECRET_MAIN,
      .google_client_id_remoting = GOOGLE_CLIENT_ID_REMOTING,
      .google_client_secret_remoting = GOOGLE_CLIENT_SECRET_REMOTING,
      .google_client_id_remoting_host = GOOGLE_CLIENT_ID_REMOTING_HOST,
      .google_client_secret_remoting_host = GOOGLE_CLIENT_SECRET_REMOTING_HOST,
      .google_default_client_id = GOOGLE_DEFAULT_CLIENT_ID,
      .google_default_client_secret = GOOGLE_DEFAULT_CLIENT_SECRET};
}
