// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
#define NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/version.h"
#include "crypto/sha2.h"
#include "net/base/net_export.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

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
};

struct NET_EXPORT ChromeRootCertConstraints {
  ChromeRootCertConstraints();
  ChromeRootCertConstraints(std::optional<base::Time> sct_not_after,
                            std::optional<base::Time> sct_all_after,
                            std::optional<base::Version> min_version,
                            std::optional<base::Version> max_version_exclusive,
                            std::vector<std::string> permitted_dns_names);
  explicit ChromeRootCertConstraints(
      const StaticChromeRootCertConstraints& constraints);
  ~ChromeRootCertConstraints();
  ChromeRootCertConstraints(const ChromeRootCertConstraints& other);
  ChromeRootCertConstraints(ChromeRootCertConstraints&& other);
  ChromeRootCertConstraints& operator=(const ChromeRootCertConstraints& other);
  ChromeRootCertConstraints& operator=(ChromeRootCertConstraints&& other);

  std::optional<base::Time> sct_not_after;
  std::optional<base::Time> sct_all_after;

  std::optional<base::Version> min_version;
  std::optional<base::Version> max_version_exclusive;

  std::vector<std::string> permitted_dns_names;
};

// ChromeRootStoreData is a container class that stores all of the Chrome Root
// Store data in a single class.
class NET_EXPORT ChromeRootStoreData {
 public:
  struct NET_EXPORT Anchor {
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints);
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints,
           bool eutl);
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints,
           bool eutl,
           bool enforce_anchor_expiry,
           bool enforce_anchor_constraints,
           std::vector<uint8_t> trust_anchor_id);
    ~Anchor();

    Anchor(const Anchor& other);
    Anchor(Anchor&& other);
    Anchor& operator=(const Anchor& other);
    Anchor& operator=(Anchor&& other);

    std::shared_ptr<const bssl::ParsedCertificate> certificate;
    std::vector<ChromeRootCertConstraints> constraints;
    bool eutl;
    bool enforce_anchor_expiry;
    // True if the certificate verifier should enforce X.509 constraints encoded
    // in the certificate.
    bool enforce_anchor_constraints;
    std::vector<uint8_t> trust_anchor_id;
  };

  // CreateFromRootStoreProto converts |proto| into a usable
  // ChromeRootStoreData object. Returns std::nullopt if the passed in
  // proto has errors in it (e.g. an unparsable DER-encoded certificate).
  static std::optional<ChromeRootStoreData> CreateFromRootStoreProto(
      const chrome_root_store::RootStore& proto);

  // Creates a ChromeRootStoreData referring to the Chrome Root Store that is
  // compiled in to the binary.
  static ChromeRootStoreData CreateFromCompiledRootStore();

  // Creates a ChromeRootStoreData using the provided test data.
  static ChromeRootStoreData CreateForTesting(
      base::span<const ChromeRootCertInfo> certs,
      base::span<const base::span<const uint8_t>> eutl_certs,
      int64_t version);

  ~ChromeRootStoreData();

  ChromeRootStoreData(const ChromeRootStoreData& other);
  ChromeRootStoreData(ChromeRootStoreData&& other);
  ChromeRootStoreData& operator=(const ChromeRootStoreData& other);
  ChromeRootStoreData& operator=(ChromeRootStoreData&& other);

  const std::vector<Anchor>& trust_anchors() const { return trust_anchors_; }
  const std::vector<Anchor>& additional_certs() const {
    return additional_certs_;
  }
  int64_t version() const { return version_; }

 private:
  ChromeRootStoreData();
  ChromeRootStoreData(base::span<const ChromeRootCertInfo> certs,
                      base::span<const base::span<const uint8_t>> eutl_certs,
                      bool certs_are_static,
                      int64_t version);

  std::vector<Anchor> trust_anchors_;
  std::vector<Anchor> additional_certs_;
  int64_t version_;
};

// TrustStoreChrome contains the Chrome Root Store, as described at
// https://g.co/chrome/root-policy
class NET_EXPORT TrustStoreChrome : public bssl::TrustStore {
 public:
  using ConstraintOverrideMap =
      base::flat_map<std::array<uint8_t, crypto::kSHA256Length>,
                     std::vector<ChromeRootCertConstraints>>;

  // Commandline switch that can be used to specify constraints for testing
  // purposes.
  //
  // The base unit of the switch is a root constraint specification:
  //   `${comma_separated_root_sha256_hashes}:${comma_separated_constraints}`
  // Multiple such specifications can be separated by `+` characters:
  //   `${hashes}:${constraints}+${morehashes}:${moreconstraints}`
  //
  // Recognized constraints:
  //   `sctnotafter=${seconds_since_epoch}`
  //   `sctallafter=${seconds_since_epoch}`
  //   `minversion=${dotted_version_string}`
  //   `maxversionexclusive=${dotted_version_string}`
  //   `dns=${permitted_dns_name}` (can be specified multiple times)
  //
  // If the same root hash is specified multiple times in separate constraint
  // specifications, each time will create a new constraintset for that root,
  // which can be used to test the handling of multiple constraintsets on one
  // root.
  static constexpr char kTestCrsConstraintsSwitch[] = "test-crs-constraints";

  // Creates a TrustStoreChrome that uses a copy of `certs`, instead of the
  // default Chrome Root Store.
  static std::unique_ptr<TrustStoreChrome> CreateTrustStoreForTesting(
      base::span<const ChromeRootCertInfo> certs,
      base::span<const base::span<const uint8_t>> eutl_certs,
      int64_t version,
      ConstraintOverrideMap override_constraints = {});

  // Creates a TrustStoreChrome that uses the compiled in Chrome Root Store.
  TrustStoreChrome();

  // Creates a TrustStoreChrome that uses the passed in anchors as
  // the contents of the Chrome Root Store.
  explicit TrustStoreChrome(const ChromeRootStoreData& anchors);
  ~TrustStoreChrome() override;

  TrustStoreChrome(const TrustStoreChrome& other) = delete;
  TrustStoreChrome& operator=(const TrustStoreChrome& other) = delete;

  // bssl::TrustStore implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

  // Returns true if the trust store contains the given bssl::ParsedCertificate
  // (matches by DER).
  bool Contains(const bssl::ParsedCertificate* cert) const;

  // Returns the root store constraints for `cert`, or an empty span if the
  // certificate is not constrained.
  base::span<const ChromeRootCertConstraints> GetConstraintsForCert(
      const bssl::ParsedCertificate* cert) const;

  int64_t version() const { return version_; }

  // Parses a string specifying constraint overrides, in the format expected by
  // the `kTestCrsConstraintsSwitch` command line switch.
  static ConstraintOverrideMap ParseCrsConstraintsSwitch(
      std::string_view switch_value);

  bssl::TrustStore* eutl_trust_store() { return &eutl_trust_store_; }

  const absl::flat_hash_set<std::vector<uint8_t>>& trust_anchor_ids() const {
    return trust_anchor_ids_;
  }

 private:
  TrustStoreChrome(const ChromeRootStoreData& root_store_data,
                   ConstraintOverrideMap override_constraints);

  static ConstraintOverrideMap InitializeConstraintsOverrides();

  bssl::TrustStoreInMemory trust_store_;

  // Map from certificate DER bytes to additional constraints (if any) for that
  // certificate. The DER bytes of the key are owned by the ParsedCertificate
  // stored in `trust_store_`, so this must be below `trust_store_` in the
  // member list.
  base::flat_map<std::string_view, std::vector<ChromeRootCertConstraints>>
      constraints_;

  // Map from certificate SHA256 hash to constraints. If a certificate has an
  // entry in this map, it will override the entry in `constraints_` (if any).
  const ConstraintOverrideMap override_constraints_;

  bssl::TrustStoreInMemory eutl_trust_store_;

  int64_t version_;

  // The set of Trust Anchor IDs associated with this trust store's TLS trust
  // anchors.
  absl::flat_hash_set<std::vector<uint8_t>> trust_anchor_ids_;
};

// Returns the version # of the Chrome Root Store that was compiled into the
// binary.
NET_EXPORT int64_t CompiledChromeRootStoreVersion();

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
