// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
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

// Adds a single `trust_anchor_id` onto the TLS-encoded
// `selected_trust_anchor_ids` list, which is a sequence of length-prefixed
// byte-strings.
void AddTrustAnchorIdToEncodedList(
    base::span<const uint8_t> trust_anchor_id,
    std::vector<uint8_t>& selected_trust_anchor_ids) {
  selected_trust_anchor_ids.emplace_back(
      base::checked_cast<uint8_t>(trust_anchor_id.size()));
  selected_trust_anchor_ids.insert(selected_trust_anchor_ids.end(),
                                   trust_anchor_id.begin(),
                                   trust_anchor_id.end());
}

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

bool SSLContextConfig::ShouldAdvertiseTrustAnchorIDs() const {
  return (base::FeatureList::IsEnabled(features::kTLSTrustAnchorIDs) &&
          (!trust_anchor_ids.empty() || !mtc_trust_anchor_ids.empty()));
}

std::vector<uint8_t> SSLContextConfig::SelectTrustAnchorIDs(
    const std::vector<std::vector<uint8_t>>& server_advertised_trust_anchor_ids)
    const {
  std::vector<uint8_t> selected_trust_anchor_ids;

  for (const auto& server_advertised_tai : server_advertised_trust_anchor_ids) {
    if (trust_anchor_ids.contains(server_advertised_tai)) {
      AddTrustAnchorIdToEncodedList(server_advertised_tai,
                                    selected_trust_anchor_ids);
    }
  }

  // In the current experiment, MTC trust anchor IDs are sent unconditionally,
  // so logic to intersect advertised IDs with MTC trust anchor ranges isn't
  // implemented. `mtc_trust_anchor_ids` is only populated when the experiment
  // is enabled, so the feature flag isn't explicitly checked here.
  for (const auto& mtc_trust_anchor_id : mtc_trust_anchor_ids) {
    AddTrustAnchorIdToEncodedList(mtc_trust_anchor_id,
                                  selected_trust_anchor_ids);
  }

  return selected_trust_anchor_ids;
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
