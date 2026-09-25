// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CHROME_ROOT_STORE_DATA_H_
#define NET_CERT_INTERNAL_CHROME_ROOT_STORE_DATA_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/time/time.h"

namespace net {

// Represents a ConstraintSet for compiled-in version of the root store.
// This is a separate struct from ChromeRootCertConstraints since the in-memory
// representation parses the version constraints into a base::Version.
// (base::Version can't be used in the compiled-in version since it isn't
// constexpr.)
struct StaticChromeRootCertConstraints {
  std::optional<base::Time> sct_not_after;
  std::optional<base::Time> sct_all_after;

  std::optional<std::string_view> min_version;
  std::optional<std::string_view> max_version_exclusive;

  base::span<const std::string_view> permitted_dns_names;

  std::optional<uint64_t> serial_not_after;
  std::optional<uint64_t> serial_after;

  std::optional<base::Time> validity_starts_not_after;
  std::optional<base::Time> validity_starts_after;
};

struct ChromeRootCertInfo {
  base::span<const uint8_t> root_cert_der;
  base::span<const StaticChromeRootCertConstraints> constraints;
  bool enforce_anchor_expiry;
  // True if the certificate verifier should enforce X.509 constraints encoded
  // in the certificate.
  bool enforce_anchor_constraints;
  // If non-empty, the binary representation of the Trust Anchor ID
  // (https://tlswg.org/tls-trust-anchor-ids/draft-ietf-tls-trust-anchor-ids.html)
  // associated with this anchor -- that is, a relative object identifier in
  // binary representation. If empty, this anchor has no associated Trust Anchor
  // ID.
  base::span<const uint8_t> trust_anchor_id;
  std::optional<int32_t> crs_root_id;
};

// Raw metadata.
struct EVMetadata {
  // kMaxOIDsPerCA is the number of OIDs that we can support per root CA. At
  // least one CA has different EV policies for business vs government
  // entities and, in the case of cross-signing, we might need to list another
  // CA's policy OID under the cross-signing root.
  static constexpr size_t kMaxOIDsPerCA = 2;

  // The SHA-256 fingerprint of the root CA certificate, used as a unique
  // identifier for a root CA certificate.
  std::array<uint8_t, 32> fingerprint;

  // The EV policy OIDs of the root CA.
  const std::string_view policy_oids[kMaxOIDsPerCA];
};

// Returns the compiled-in Chrome Root Store data.
base::span<const ChromeRootCertInfo> GetCompiledChromeRootCertList();
base::span<const base::span<const uint8_t>> GetCompiledEutlRootCertList();
int64_t GetCompiledChromeRootStoreVersion();

// Returns the compiled-in Signer Set data.
int64_t GetCompiledSignerSetTimestampSeconds();
base::span<const uint8_t> GetCompiledSignerSetProtoBytes();
std::optional<base::span<const uint8_t>> FindCompiledSignerKey(
    base::span<const uint8_t> sha256_hash);

// Returns the compiled-in EV Root CA metadata.
base::span<const EVMetadata> GetEvRootCaMetadata();

}  // namespace net

#endif  // NET_CERT_INTERNAL_CHROME_ROOT_STORE_DATA_H_
