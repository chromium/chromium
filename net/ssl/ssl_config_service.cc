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

// Intersects the set of supported `trust_anchor_ids` with the server's
// `server_advertised_trust_anchor_ids`, adding the matches into
// `selected_trust_anchor_ids`.
void AddIntersectingTrustAnchorIdsToEncodedList(
    absl::flat_hash_set<std::vector<uint8_t>> trust_anchor_ids,
    const std::vector<std::vector<uint8_t>>& server_advertised_trust_anchor_ids,
    std::vector<uint8_t>& selected_trust_anchor_ids) {
  for (const auto& server_advertised_tai : server_advertised_trust_anchor_ids) {
    if (trust_anchor_ids.contains(server_advertised_tai)) {
      AddTrustAnchorIdToEncodedList(server_advertised_tai,
                                    selected_trust_anchor_ids);
    }
  }
}

void AddIntersectingMTCTrustAnchorIdsToEncodedList(
    const std::vector<std::vector<uint8_t>>& mtc_trust_anchor_ids,
    const std::vector<std::vector<uint8_t>>& server_advertised_trust_anchor_ids,
    std::vector<uint8_t>& selected_trust_anchor_ids) {
  // TODO(crbug.com/452986179): consider plumbing the MTC trust anchor ids into
  // SSLContextConfig in decomposed form, so that we don't need to parse them
  // back into separate base id and landmark number here. (And could plumb in
  // the minimum landmark too, in order to do the full comparison.)

  // Hasher that allows heterogeneous lookup from span<const uint8_t>.
  struct ByteSpanHash
      : absl::DefaultHashContainerHash<base::span<const uint8_t>> {
    using is_transparent = void;
  };

  // Construct mapping of trusted MTC anchor base_id to maximum supported
  // landmark number for that anchor.
  absl::flat_hash_map<std::vector<uint8_t>, uint64_t, ByteSpanHash,
                      std::ranges::equal_to>
      mtc_base_id_landmarks;
  for (const auto& tai : mtc_trust_anchor_ids) {
    auto split = x509_util::SplitLastOidComponent(tai);
    if (!split) {
      continue;
    }
    mtc_base_id_landmarks[base::ToVector(split->base_id)] =
        split->last_component;
  }

  // For each server advertised TAI, check if it matches a trusted MTC anchor
  // and is within the known landmark range.
  for (const auto& server_tai : server_advertised_trust_anchor_ids) {
    auto split_server_tai = x509_util::SplitLastOidComponent(server_tai);
    if (!split_server_tai) {
      continue;
    }
    auto matching_known_mtc =
        mtc_base_id_landmarks.find(split_server_tai->base_id);
    if (matching_known_mtc != mtc_base_id_landmarks.end() &&
        split_server_tai->last_component <= matching_known_mtc->second) {
      AddTrustAnchorIdToEncodedList(
          x509_util::AppendOidComponent(matching_known_mtc->first,
                                        matching_known_mtc->second),
          selected_trust_anchor_ids);

      // Remove the entry from the trusted MTC anchors map, so that we don't
      // include the same TAI again in the result if multiple server TAIs match
      // the same trusted TAI.
      mtc_base_id_landmarks.erase(matching_known_mtc);
    }
  }
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

bool SSLContextConfig::ShouldAdvertiseTrustAnchorIDs() const {
  return (base::FeatureList::IsEnabled(features::kTLSTrustAnchorIDs) &&
          (!trust_anchor_ids.empty() || !mtc_trust_anchor_ids.empty()));
}

std::vector<uint8_t> SSLContextConfig::SelectTrustAnchorIDs(
    const std::vector<std::vector<uint8_t>>& server_advertised_trust_anchor_ids)
    const {
  std::vector<uint8_t> selected_trust_anchor_ids;

  AddIntersectingTrustAnchorIdsToEncodedList(trust_anchor_ids,
                                             server_advertised_trust_anchor_ids,
                                             selected_trust_anchor_ids);

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

std::optional<std::vector<uint8_t>>
SSLContextConfig::SelectTrustAnchorIDsForRetry(
    X509Certificate* server_cert,
    const std::vector<std::vector<uint8_t>>& server_advertised_trust_anchor_ids,
    bool* used_mtc_fallback) const {
  std::vector<uint8_t> selected_trust_anchor_ids;

  *used_mtc_fallback = false;

  AddIntersectingTrustAnchorIdsToEncodedList(trust_anchor_ids,
                                             server_advertised_trust_anchor_ids,
                                             selected_trust_anchor_ids);

  // TODO(crbug.com/432044228): It should be possible to implement a general TAI
  // fallback for the case where we requested a TAI on the initial connection,
  // the server sent us a certificate that matched that TAI, but it failed to
  // verify. In that case we could do the intersection to calculate the
  // retry list like in the normal retry, but exclude the TAI for the cert that
  // failed to verify. There is some complexity in figuring out which TAI
  // matched the certificate the server sent, but that should be possible to
  // figure out by looking for a matching issuer name in the root store and
  // checking the TAI for that anchor, except that root store information isn't
  // available at this layer.
  // If this is done it could replace the special-case MTC fallback here since
  // it should be able to handle that case too.
  if (server_cert->signature_algorithm() ==
      bssl::SignatureAlgorithm::kMtcProofDraftDavidben08) {
    // If the server sent a signatureless MTC certificate and it failed to
    // verify, retry the connection without advertising the MTC trust anchor
    // IDs. Note that this will intentionally even retry the connection with an
    // empty trust anchor ID list (the assumption being that if the MTC failed
    // to verify successfully, the server should also be configured with a
    // default cert that is not an MTC and which might work.)
    *used_mtc_fallback = true;
    return selected_trust_anchor_ids;
  }

  AddIntersectingMTCTrustAnchorIdsToEncodedList(
      mtc_trust_anchor_ids, server_advertised_trust_anchor_ids,
      selected_trust_anchor_ids);

  if (selected_trust_anchor_ids.empty()) {
    // If there is no intersection between the supported trust anchor IDs and
    // those that the server advertised, don't retry.
    return std::nullopt;
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
