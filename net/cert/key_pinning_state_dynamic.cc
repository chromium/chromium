// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_dynamic.h"

#include "net/dns/dns_names_util.h"

namespace net {
namespace {

KeyPinningState::HashedHost HashHost(
    base::span<const uint8_t> canonicalized_host) {
  return crypto::hash::Sha256(canonicalized_host);
}

}  // namespace

KeyPinningStateDynamic::KeyPinningStateDynamic() = default;
KeyPinningStateDynamic::~KeyPinningStateDynamic() = default;

void KeyPinningStateDynamic::AddHPKP(std::string_view host,
                                     base::Time expiry,
                                     bool include_subdomains,
                                     const HashValueVector& hashes) {
  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty()) {
    return;
  }

  PKPState pkp_state;
  pkp_state.expiry = expiry;
  pkp_state.include_subdomains = include_subdomains;
  for (const auto& hash : hashes) {
    if (hash.tag() == HASH_VALUE_SHA256) {
      pkp_state.spki_hashes.insert(hash.sha256hashvalue());
    }
  }

  // Only store new state when HPKP is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (pkp_state.HasPublicKeyPins()) {
    enabled_pkp_hosts_[HashHost(canonicalized_host)] = pkp_state;
  } else {
    const HashedHost hashed_host = HashHost(canonicalized_host);
    enabled_pkp_hosts_.erase(hashed_host);
  }
}

bool KeyPinningStateDynamic::GetPKPState(std::string_view host,
                                         PKPState* result) const {
  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty()) {
    return false;
  }

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    base::span<const uint8_t> host_sub_chunk =
        base::span(canonicalized_host).subspan(i);
    auto j = enabled_pkp_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_pkp_hosts_.end()) {
      continue;
    }

    // If the entry is invalid, skip it.
    if (current_time > j->second.expiry) {
      continue;
    }

    // If this is the most specific PKP match, add it to the result. Note: a PKP
    // entry at a more specific domain overrides a less specific domain whether
    // or not |include_subdomains| is set.
    //
    // TODO(davidben): This does not match the HSTS behavior. We no longer
    // implement HPKP, so this logic is only used via AddHPKP(), reachable from
    // Cronet.
    if (i == 0 || j->second.include_subdomains) {
      *result = j->second;
      return true;
    }

    break;
  }

  return false;
}

}  // namespace net
