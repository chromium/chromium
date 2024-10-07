// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_API_KEY_CACHE_H_
#define GOOGLE_APIS_API_KEY_CACHE_H_

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/buildflags.h"
#include "google_apis/google_api_keys.h"

namespace google_apis {

struct DefaultApiKeys;

COMPONENT_EXPORT(GOOGLE_APIS) BASE_DECLARE_FEATURE(kOverrideAPIKeyFeature);

// This is used as a lazy instance to determine keys once and cache them.
class COMPONENT_EXPORT(GOOGLE_APIS) ApiKeyCache {
 public:
  explicit ApiKeyCache(const DefaultApiKeys& default_api_keys);

  ApiKeyCache(const ApiKeyCache&) = delete;
  ApiKeyCache& operator=(const ApiKeyCache&) = delete;

  ~ApiKeyCache();

  const std::string& api_key() const { return api_key_; }
#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
  void set_api_key(const std::string& api_key) { api_key_ = api_key; }
#endif
  const std::string& api_key_non_stable() const { return api_key_non_stable_; }
  const std::string& api_key_remoting() const { return api_key_remoting_; }
  const std::string& api_key_soda() const { return api_key_soda_; }
#if !BUILDFLAG(IS_ANDROID)
  const std::string& api_key_hats() const { return api_key_hats_; }
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string& api_key_sharing() const { return api_key_sharing_; }
  const std::string& api_key_read_aloud() const { return api_key_read_aloud_; }
  const std::string& api_key_fresnel() const { return api_key_fresnel_; }
  const std::string& api_key_boca() const { return api_key_boca_; }
#endif

  const std::string& metrics_key() const { return metrics_key_; }

  const std::string& GetClientID(OAuth2Client client) const;
#if BUILDFLAG(IS_IOS)
  void SetClientID(OAuth2Client client, const std::string& client_id);
#endif

  const std::string& GetClientSecret(OAuth2Client client) const;
#if BUILDFLAG(IS_IOS)
  void SetClientSecret(OAuth2Client client, const std::string& client_secret);
#endif

  bool HasAPIKeyConfigured() const;
  bool HasOAuthClientConfigured() const;

 private:
  std::string api_key_;
  std::string api_key_non_stable_;
  std::string api_key_remoting_;
  std::string api_key_soda_;
#if !BUILDFLAG(IS_ANDROID)
  std::string api_key_hats_;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string api_key_sharing_;
  std::string api_key_read_aloud_;
  std::string api_key_fresnel_;
  std::string api_key_boca_;
#endif

  std::string metrics_key_;
  std::string client_ids_[CLIENT_NUM_ITEMS];
  std::string client_secrets_[CLIENT_NUM_ITEMS];
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_API_KEY_CACHE_H_
