// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <tuple>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "net/base/features.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"

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

std::optional<uint16_t> SSLContextConfig::RequestServerPadding() const {
  if (!base::FeatureList::IsEnabled(features::kAddTLSServerHandshakePadding)) {
    return std::nullopt;
  }
  return base::saturated_cast<uint16_t>(
      features::kAddTLSServerHandshakePaddingBytes.Get());
}

bool SSLContextConfig::ShouldAdvertiseTrustAnchorIDs() const {
  if (!base::FeatureList::IsEnabled(features::kTLSTrustAnchorIDs)) {
    return false;
  }
  bool has_non_mtc_advertisable =
      base::FeatureList::IsEnabled(features::kNonMtcTrustAnchorIDs) &&
      !trust_anchor_ids.empty();
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  bool has_mtc_advertisable =
      base::FeatureList::IsEnabled(features::kVerifyMTCs) &&
      !mtc_trust_anchor_ids.empty();
#else
  bool has_mtc_advertisable = false;
#endif
  return has_non_mtc_advertisable || has_mtc_advertisable;
}

std::vector<uint8_t> SSLContextConfig::SelectAllTrustAnchorIDs() const {
  std::vector<uint8_t> selected_trust_anchor_ids;

  if (base::FeatureList::IsEnabled(features::kTLSTrustAnchorIDs)) {
    if (base::FeatureList::IsEnabled(features::kNonMtcTrustAnchorIDs)) {
      for (const auto& trust_anchor_id : trust_anchor_ids) {
        AddTrustAnchorIdToEncodedList(trust_anchor_id,
                                      selected_trust_anchor_ids);
      }
    }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    if (base::FeatureList::IsEnabled(features::kVerifyMTCs)) {
      for (const auto& mtc_trust_anchor_id : mtc_trust_anchor_ids) {
        AddTrustAnchorIdToEncodedList(mtc_trust_anchor_id,
                                      selected_trust_anchor_ids);
      }
    }
#endif
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
