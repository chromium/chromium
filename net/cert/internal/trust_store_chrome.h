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
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/boringssl/src/pki/path_builder.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace chrome_root_store {
class RootStore;
class MtcMetadata;
}

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

  std::optional<uint64_t> index_not_after;
  std::optional<uint64_t> index_after;

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
};

struct ChromeMtcAnchorInfo {
  base::span<const uint8_t> log_id;
  base::span<const StaticChromeRootCertConstraints> constraints;
  // Does not contain `tls_trust_anchor`, as MtcAnchors without that set to
  // true are simply ignored.
};

struct NET_EXPORT ChromeRootCertConstraints {
  ChromeRootCertConstraints();
  ChromeRootCertConstraints(std::optional<base::Time> sct_not_after,
                            std::optional<base::Time> sct_all_after,
                            std::optional<base::Version> min_version,
                            std::optional<base::Version> max_version_exclusive,
                            std::vector<std::string> permitted_dns_names,
                            std::optional<uint64_t> index_not_after,
                            std::optional<uint64_t> index_after,
                            std::optional<base::Time> validity_starts_not_after,
                            std::optional<base::Time> validity_starts_after);
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

  std::optional<uint64_t> index_not_after;
  std::optional<uint64_t> index_after;

  std::optional<base::Time> validity_starts_not_after;
  std::optional<base::Time> validity_starts_after;
};

// ChromeRootStoreData is a container class that stores the Chrome Root Store
// data which is updated by the RootStore and MtcMetadata protos.
class NET_EXPORT ChromeRootStoreData {
 public:
  struct NET_EXPORT Anchor {
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints);
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints,
           bool enforce_anchor_expiry,
           bool enforce_anchor_constraints);
    ~Anchor();

    Anchor(const Anchor& other);
    Anchor(Anchor&& other);
    Anchor& operator=(const Anchor& other);
    Anchor& operator=(Anchor&& other);

    std::shared_ptr<const bssl::ParsedCertificate> certificate;
    std::vector<ChromeRootCertConstraints> constraints;
    bool enforce_anchor_expiry;
    // True if the certificate verifier should enforce X.509 constraints encoded
    // in the certificate.
    bool enforce_anchor_constraints;
  };

  struct NET_EXPORT MtcAnchor {
    MtcAnchor(std::vector<uint8_t> log_id,
              std::vector<ChromeRootCertConstraints> constraints);
    ~MtcAnchor();

    MtcAnchor(const MtcAnchor& other);
    MtcAnchor(MtcAnchor&& other);
    MtcAnchor& operator=(const MtcAnchor& other);
    MtcAnchor& operator=(MtcAnchor&& other);

    std::vector<uint8_t> log_id;
    std::vector<ChromeRootCertConstraints> constraints;
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
      base::span<const ChromeMtcAnchorInfo> mtc_anchors,
      int64_t version);

  ~ChromeRootStoreData();

  ChromeRootStoreData(const ChromeRootStoreData& other);
  ChromeRootStoreData(ChromeRootStoreData&& other);
  ChromeRootStoreData& operator=(const ChromeRootStoreData& other);
  ChromeRootStoreData& operator=(ChromeRootStoreData&& other);

  const std::vector<Anchor>& trust_anchors() const { return trust_anchors_; }
  const std::vector<Anchor>& eutl_certs() const { return eutl_certs_; }
  const std::vector<MtcAnchor>& mtc_trust_anchors() const {
    return mtc_trust_anchors_;
  }
  int64_t version() const { return version_; }

 private:
  ChromeRootStoreData();
  ChromeRootStoreData(base::span<const ChromeRootCertInfo> certs,
                      base::span<const base::span<const uint8_t>> eutl_certs,
                      base::span<const ChromeMtcAnchorInfo> mtc_anchors,
                      bool certs_are_static,
                      int64_t version);

  std::vector<Anchor> trust_anchors_;
  std::vector<Anchor> eutl_certs_;
  std::vector<MtcAnchor> mtc_trust_anchors_;
  int64_t version_;
};

// ChromeRootStoreMtcMetadata is a container class that stores the Chrome Root
// Store data which is updated by the MtcMetadata proto.
class NET_EXPORT ChromeRootStoreMtcMetadata {
 public:
  struct NET_EXPORT MtcAnchorData {
    MtcAnchorData();
    ~MtcAnchorData();
    MtcAnchorData(const MtcAnchorData& other);
    MtcAnchorData(MtcAnchorData&& other);
    MtcAnchorData& operator=(const MtcAnchorData& other);
    MtcAnchorData& operator=(MtcAnchorData&& other);

    std::vector<uint8_t> log_id;

    // The landmark info isn't needed in the verifier, but keep track of it so
    // that it can be displayed in the root store UI.
    std::vector<uint8_t> landmark_base_id;
    uint64_t landmark_min_inclusive;
    uint64_t landmark_max_inclusive;

    std::vector<bssl::TrustedSubtree> trusted_subtrees;

    // The revocation map key is the end index (exclusive) and the value is the
    // start index (inclusive).
    base::flat_map<uint64_t, uint64_t> revoked_indices;
  };

  // CreateFromMtcMetadataProto converts |proto| into a usable
  // ChromeRootStoreMtcMetadata object. Returns std::nullopt if the passed in
  // proto has errors in it.
  static std::optional<ChromeRootStoreMtcMetadata> CreateFromMtcMetadataProto(
      const chrome_root_store::MtcMetadata& proto);

  ~ChromeRootStoreMtcMetadata();

  ChromeRootStoreMtcMetadata(const ChromeRootStoreMtcMetadata& other);
  ChromeRootStoreMtcMetadata(ChromeRootStoreMtcMetadata&& other);
  ChromeRootStoreMtcMetadata& operator=(
      const ChromeRootStoreMtcMetadata& other);
  ChromeRootStoreMtcMetadata& operator=(ChromeRootStoreMtcMetadata&& other);

  const absl::flat_hash_map<std::vector<uint8_t>, MtcAnchorData>&
  mtc_anchor_data() const {
    return mtc_anchor_data_;
  }
  base::Time update_time() const { return update_time_; }

 private:
  ChromeRootStoreMtcMetadata();

  // Map from a Merkle Tree Anchor log_id to the data for that anchor.
  absl::flat_hash_map<std::vector<uint8_t>, MtcAnchorData> mtc_anchor_data_;
  base::Time update_time_;
};

// TrustStoreChrome contains the Chrome Root Store, as described at
// https://g.co/chrome/root-policy
class NET_EXPORT TrustStoreChrome : public bssl::TrustStore {
 public:
  using ConstraintOverrideMap =
      base::flat_map<std::array<uint8_t, crypto::kSHA256Length>,
                     std::vector<ChromeRootCertConstraints>>;

  // Additional data about MTC anchors that isn't represented in
  // bssl::MTCAnchor.
  struct NET_EXPORT MtcAnchorExtraData {
    MtcAnchorExtraData();
    ~MtcAnchorExtraData();
    MtcAnchorExtraData(const MtcAnchorExtraData& other);
    MtcAnchorExtraData(MtcAnchorExtraData&& other);
    MtcAnchorExtraData& operator=(const MtcAnchorExtraData& other);
    MtcAnchorExtraData& operator=(MtcAnchorExtraData&& other);

    // The revocation map key is the end index (exclusive) and the value is the
    // start index (inclusive).
    base::flat_map<uint64_t, uint64_t> revoked_indices;

    std::vector<ChromeRootCertConstraints> constraints;

    // TODO(crbug.com/452986180): support constraint overrides for MTC anchors.
  };

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
  // TODO(crbug.com/452986180): support constraint overrides for MTC index
  // constraints.
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

  // Returns the list of TLS Trust Anchor IDs from the compiled-in root store.
  // If |cert_list_for_testing| is non-empty, it will override the compiled-in
  // production root store.
  static std::vector<std::vector<uint8_t>>
  GetTrustAnchorIDsFromCompiledInRootStore(
      base::span<const ChromeRootCertInfo> cert_list_for_testing = {});

  // Returns the list of MTC log IDs from the compiled-in root store.
  // If |anchor_list_for_testing| is non-empty, it will override the
  // compiled-in production root store.
  static std::vector<std::vector<uint8_t>>
  GetTrustedMtcLogIDsFromCompiledInRootStore(
      base::span<const ChromeMtcAnchorInfo> anchor_list_for_testing = {});

  // Creates a TrustStoreChrome that uses the compiled in Chrome Root Store.
  TrustStoreChrome();

  // Creates a TrustStoreChrome that uses the passed in `root_store_data` and
  // `mtc_metadata` as the contents of the Chrome Root Store, if specified.
  // Either or both of the arguments may be present or null.
  TrustStoreChrome(const ChromeRootStoreData* root_store_data,
                   const ChromeRootStoreMtcMetadata* mtc_metadata);

  ~TrustStoreChrome() override;

  TrustStoreChrome(const TrustStoreChrome& other) = delete;
  TrustStoreChrome& operator=(const TrustStoreChrome& other) = delete;

  // bssl::TrustStore implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;
  std::shared_ptr<const bssl::MTCAnchor> GetTrustedMTCIssuerOf(
      const bssl::ParsedCertificate* cert) override;

  // Returns true if the trust store contains the given bssl::ParsedCertificate
  // (matches by DER).
  bool Contains(const bssl::ParsedCertificate* cert) const;
  bool ContainsMTCAnchor(const bssl::MTCAnchor* anchor) const;

  // Returns the root store constraints for `path`, or an empty span if the
  // certificate is not constrained.
  base::span<const ChromeRootCertConstraints> GetConstraintsForCert(
      const bssl::CertPathBuilderResultPath* path) const;

  // Returns additional data about the MTC anchor with log id `log_id`, or null
  // if the anchor isn't known or has no additional data.
  const MtcAnchorExtraData* GetMTCAnchorData(
      base::span<const uint8_t> log_id) const;

  int64_t version() const { return version_; }
  std::optional<base::Time> mtc_metadata_update_time() const {
    return mtc_metadata_update_time_;
  }

  // Parses a string specifying constraint overrides, in the format expected by
  // the `kTestCrsConstraintsSwitch` command line switch.
  static ConstraintOverrideMap ParseCrsConstraintsSwitch(
      std::string_view switch_value);

  bssl::TrustStore* eutl_trust_store() { return &eutl_trust_store_; }

 private:
  TrustStoreChrome(const ChromeRootStoreData& root_store_data,
                   const ChromeRootStoreMtcMetadata* mtc_metadata,
                   ConstraintOverrideMap override_constraints);

  static ConstraintOverrideMap InitializeConstraintsOverrides();
  base::span<const ChromeRootCertConstraints> GetConstraintsForMTC(
      const bssl::MTCAnchor* mtc_anchor) const;
  base::span<const ChromeRootCertConstraints> GetConstraintsForClassicalCert(
      const bssl::ParsedCertificate* cert) const;

  bssl::TrustStoreInMemory trust_store_;

  // Hasher that allows heterogeneous lookup from span<const uint8_t>.
  struct ByteSpanHash
      : absl::DefaultHashContainerHash<base::span<const uint8_t>> {
    using is_transparent = void;
  };
  // Map from log_id to additional data for the MTC anchor with the
  // matching log id. This stores data that isn't handled in bssl:MTCAnchor.
  absl::flat_hash_map<std::vector<uint8_t>,
                      MtcAnchorExtraData,
                      ByteSpanHash,
                      std::ranges::equal_to>
      mtc_anchor_extra_data_;

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

  std::optional<base::Time> mtc_metadata_update_time_;
};

// Returns the version # of the Chrome Root Store that was compiled into the
// binary.
NET_EXPORT int64_t CompiledChromeRootStoreVersion();

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
