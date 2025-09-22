// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "net/base/features.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// The default NamedGroups supported by Chromium.
// This default list matches the result of prepending our preferred post-quantum
// group (X25519MLKEM768) to BoringSSL's kDefaultSupportedGroupIds and
// evaluating BoringSSL's default logic for selecting key shares.
const SSLNamedGroupInfo kDefaultSSLSupportedGroups[] = {
    {.group_id = SSL_GROUP_X25519_MLKEM768, .send_key_share = true},
    {.group_id = SSL_GROUP_X25519, .send_key_share = true},
    {.group_id = SSL_GROUP_SECP256R1, .send_key_share = false},
    {.group_id = SSL_GROUP_SECP384R1, .send_key_share = false},
};

}  // namespace

// This function should be kept updated to include all the post-quantum groups
// that //net and callers know about and may configure.
bool SSLNamedGroupInfo::IsPostQuantum() const {
  return group_id == SSL_GROUP_X25519_MLKEM768 ||
         group_id == SSL_GROUP_MLKEM1024;
}

SSLContextConfig::SSLContextConfig() {
  supported_named_groups.assign(std::begin(kDefaultSSLSupportedGroups),
                                std::end(kDefaultSSLSupportedGroups));
}

SSLContextConfig::SSLContextConfig(const SSLContextConfig&) = default;
SSLContextConfig::SSLContextConfig(SSLContextConfig&&) = default;
SSLContextConfig::~SSLContextConfig() = default;
SSLContextConfig& SSLContextConfig::operator=(const SSLContextConfig&) =
    default;
SSLContextConfig& SSLContextConfig::operator=(SSLContextConfig&&) = default;
bool SSLContextConfig::operator==(const SSLContextConfig&) const = default;

std::vector<uint16_t> SSLContextConfig::GetSupportedGroups(
    bool key_shares_only) const {
  std::vector<uint16_t> groups_out;
  for (const SSLNamedGroupInfo& group : supported_named_groups) {
    if (!key_shares_only || group.send_key_share) {
      groups_out.push_back(group.group_id);
    }
  }
  return groups_out;
}

SSLConfigService::SSLConfigService()
    : observer_list_(base::ObserverListPolicy::EXISTING_ONLY) {}

SSLConfigService::~SSLConfigService() = default;

void SSLConfigService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SSLConfigService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SSLConfigService::NotifySSLContextConfigChange() {
  for (auto& observer : observer_list_)
    observer.OnSSLContextConfigChanged();
}

void SSLConfigService::ProcessConfigUpdate(const SSLContextConfig& old_config,
                                           const SSLContextConfig& new_config,
                                           bool force_notification) {
  // Do nothing if the configuration hasn't changed.
  if (old_config != new_config || force_notification) {
    NotifySSLContextConfigChange();
  }
}

}  // namespace net
