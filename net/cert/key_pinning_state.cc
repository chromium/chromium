// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state.h"

#include "net/dns/dns_names_util.h"

namespace net {

namespace {

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
bool HashesIntersect(const absl::flat_hash_set<SHA256HashValue>& a,
                     const std::vector<SHA256HashValue>& b) {
  for (const auto& hash : b) {
    if (a.contains(hash)) {
      return true;
    }
  }
  return false;
}

}  // namespace

KeyPinningState::KeyPinningState() = default;
KeyPinningState::~KeyPinningState() = default;

// TODO(crbug.com/497882860): this is duplicated from the same named function
// in transport_security_state.cc. Dedupe?
//
// Converts |hostname| from dotted form ("www.google.com") to the form
// used in DNS: "\x03www\x06google\x03com", lowercases that, and returns
// the result.
std::vector<uint8_t> KeyPinningState::CanonicalizeHost(std::string_view host) {
  // We cannot perform the operations as detailed in the spec here as `host`
  // has already undergone IDN processing before it reached us. Thus, we
  // lowercase the input (probably redundant since most input here has been
  // lowercased through URL canonicalization) and check that there are no
  // invalid characters in the host (via DNSDomainFromDot()).
  std::string lowered_host = base::ToLowerASCII(host);

  std::optional<std::vector<uint8_t>> new_host =
      dns_names_util::DottedNameToNetwork(
          lowered_host,
          /*require_valid_internet_hostname=*/true);
  if (!new_host.has_value()) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::vector<uint8_t>();
  }

  return std::move(new_host).value();
}

KeyPinningState::PKPStatus KeyPinningState::CheckPublicKeyPins(
    std::string_view host,
    bool is_issued_by_known_root,
    const std::vector<SHA256HashValue>& public_key_hashes) const {
  // Perform pin validation only if the server actually has public key pins.
  if (!HasPublicKeyPins(host)) {
    return PKPStatus::OK;
  }

  return CheckPublicKeyPinsImpl(host, is_issued_by_known_root,
                                public_key_hashes);
}

bool KeyPinningState::HasPublicKeyPins(std::string_view host) const {
  PKPState pkp_state;
  return GetPKPState(host, &pkp_state) && pkp_state.HasPublicKeyPins();
}

KeyPinningState::PKPStatus KeyPinningState::CheckPublicKeyPinsImpl(
    std::string_view host,
    bool is_issued_by_known_root,
    const std::vector<SHA256HashValue>& hashes) const {
  PKPState pkp_state;
  bool found_state = GetPKPState(host, &pkp_state);

  // HasPublicKeyPins should have returned true in order for this method to have
  // been called.
  DCHECK(found_state);

  if (pkp_state.CheckPublicKeyPins(hashes)) {
    return PKPStatus::OK;
  }

  // Don't report violations for certificates that chain to local roots.
  if (!is_issued_by_known_root && enable_pkp_bypass_for_local_trust_anchors_) {
    return PKPStatus::BYPASSED;
  }

  return PKPStatus::VIOLATED;
}

KeyPinningState::PKPState::PKPState() = default;

KeyPinningState::PKPState::PKPState(const PKPState& other) = default;

KeyPinningState::PKPState::~PKPState() = default;

bool KeyPinningState::PKPState::CheckPublicKeyPins(
    const std::vector<SHA256HashValue>& hashes) const {
  // Validate that hashes is not empty. By the time this code is called (in
  // production), that should never happen, but it's good to be defensive.
  // And, hashes *can* be empty in some test scenarios.
  if (hashes.empty()) {
    return false;
  }

  if (HashesIntersect(bad_spki_hashes, hashes)) {
    return false;
  }

  // If there are no pins, then any valid chain is acceptable.
  if (spki_hashes.empty()) {
    return true;
  }

  if (HashesIntersect(spki_hashes, hashes)) {
    return true;
  }

  return false;
}

bool KeyPinningState::PKPState::HasPublicKeyPins() const {
  return !spki_hashes.empty() || !bad_spki_hashes.empty();
}

KeyPinningStateDummy::~KeyPinningStateDummy() = default;

bool KeyPinningStateDummy::GetPKPState(std::string_view host,
                                       PKPState* pkp_result) const {
  return false;
}

}  // namespace net
