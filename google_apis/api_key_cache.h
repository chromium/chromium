// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_API_KEY_CACHE_H_
#define GOOGLE_APIS_API_KEY_CACHE_H_

#include <array>
#include <string>

#include "build/build_config.h"
#include "google_apis/buildflags.h"
#include "google_apis/google_api_keys.h"

namespace google_apis {

struct DefaultApiKeys;

// This is used as a lazy instance to determine keys once and cache them.
class COMPONENT_EXPORT(GOOGLE_APIS) ApiKeyCache {
 public:
  explicit ApiKeyCache(const DefaultApiKeys& default_api_keys);

  ApiKeyCache(const ApiKeyCache&) = delete;
  ApiKeyCache& operator=(const ApiKeyCache&) = delete;

  ~ApiKeyCache();

  const std::string& api_key() const { return api_key_; }
  const std::string& api_key_non_stable() const { return api_key_non_stable_; }
  const std::string& api_key_remoting() const { return api_key_remoting_; }
  const std::string& api_key_soda() const { return api_key_soda_; }
#if !BUILDFLAG(IS_ANDROID)
  const std::string& api_key_hats() const { return api_key_hats_; }
#endif
#if BUILDFLAG(IS_CHROMEOS)
  const std::string& api_key_sharing() const { return api_key_sharing_; }
  const std::string& api_key_read_aloud() const { return api_key_read_aloud_; }
  const std::string& api_key_fresnel() const { return api_key_fresnel_; }
  const std::string& api_key_boca() const { return api_key_boca_; }
  const std::string& api_key_cros_system_geo() const {
    return api_key_cros_system_geo_;
  }
  const std::string& api_key_cros_chrome_geo() const {
    return api_key_cros_chrome_geo_;
  }
#endif

  const std::string& metrics_key() const { return metrics_key_; }

  const std::string& GetClientID(OAuth2Client client) const;
  const std::string& GetClientSecret(OAuth2Client client) const;

  bool HasAPIKeyConfigured() const;
  bool HasOAuthClientConfigured() const;

#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
  void set_api_key(const std::string& api_key) { api_key_ = api_key; }
  void SetClientID(OAuth2Client client, const std::string& client_id);
  void SetClientSecret(OAuth2Client client, const std::string& client_secret);
#endif

 private:
  std::string api_key_;
  std::string api_key_non_stable_;
  std::string api_key_remoting_;
  std::string api_key_soda_;
#if !BUILDFLAG(IS_ANDROID)
  std::string api_key_hats_;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  std::string api_key_sharing_;
  std::string api_key_read_aloud_;
  std::string api_key_fresnel_;
  std::string api_key_boca_;
  std::string api_key_cros_system_geo_;
  std::string api_key_cros_chrome_geo_;
#endif

  std::string metrics_key_;
  std::array<std::string, CLIENT_NUM_ITEMS> client_ids_;
  std::array<std::string, CLIENT_NUM_ITEMS> client_secrets_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_API_KEY_CACHE_H_
