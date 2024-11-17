// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DEFAULT_API_KEYS_H_
#define GOOGLE_APIS_DEFAULT_API_KEYS_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace google_apis {

// A trivial struct bundling default API key values defined at compile time
// through preprocessor directives. Useful for injecting these values for
// testing.
struct DefaultApiKeys {
  // Used to indicate an unset key/id/secret.  This works better with
  // various unit tests than leaving the token empty.
  static constexpr char kUnsetApiToken[] = "dummytoken";

  bool allow_unset_values;
  bool allow_override_via_environment;

  const char* google_api_key;
  const char* google_metrics_signing_key;
#if BUILDFLAG(IS_ANDROID)
  const char* google_api_key_android_non_stable;
#else
  const char* google_api_key_hats;
#endif  // BUILDFLAG(IS_ANDROID)
  const char* google_api_key_remoting;
  const char* google_api_key_soda;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const char* google_api_key_sharing;
  const char* google_api_key_read_aloud;
  const char* google_api_key_fresnel;
  const char* google_api_key_boca;
#endif

  const char* google_client_id_main;
  const char* google_client_secret_main;

  const char* google_client_id_remoting;
  const char* google_client_secret_remoting;

  const char* google_client_id_remoting_host;
  const char* google_client_secret_remoting_host;

  const char* google_default_client_id;
  const char* google_default_client_secret;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DEFAULT_API_KEYS_H_
