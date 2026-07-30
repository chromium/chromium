// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/root_store_proto_lite/signer_set.pb.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/path_builder.h"

namespace net {

namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-inc.cc"
#include "net/data/ssl/chrome_root_store/signer-set-inc.cc"

std::optional<bssl::SignatureAlgorithm>
SignerSignatureAlgorithmToBsslSignatureAlgorithm(
    chrome_root_store::SignatureAlgorithm signature_algorithm) {
  switch (signature_algorithm) {
    case chrome_root_store::SIGNATURE_ALGORITHM_ML_DSA44:
      return bssl::SignatureAlgorithm::kMldsa44;
    case chrome_root_store::SIGNATURE_ALGORITHM_ML_DSA65:
      return bssl::SignatureAlgorithm::kMldsa65;
    case chrome_root_store::SIGNATURE_ALGORITHM_ML_DSA87:
      return bssl::SignatureAlgorithm::kMldsa87;
    default:
      return std::nullopt;
  }
}

}  // namespace

ChromeRootCertConstraints::ChromeRootCertConstraints() = default;
ChromeRootCertConstraints::ChromeRootCertConstraints(
    std::optional<base::Time> sct_not_after,
    std::optional<base::Time> sct_all_after,
    std::optional<base::Version> min_version,
    std::optional<base::Version> max_version_exclusive,
    std::vector<std::string> permitted_dns_names,
    std::optional<uint64_t> index_not_after,
    std::optional<uint64_t> index_after,
    std::optional<base::Time> validity_starts_not_after,
    std::optional<base::Time> validity_starts_after)
    : sct_not_after(sct_not_after),
      sct_all_after(sct_all_after),
      min_version(std::move(min_version)),
      max_version_exclusive(std::move(max_version_exclusive)),
      permitted_dns_names(std::move(permitted_dns_names)),
      index_not_after(index_not_after),
      index_after(index_after),
      validity_starts_not_after(validity_starts_not_after),
      validity_starts_after(validity_starts_after) {}

ChromeRootCertConstraints::ChromeRootCertConstraints(
    const StaticChromeRootCertConstraints& constraints)
    : sct_not_after(constraints.sct_not_after),
      sct_all_after(constraints.sct_all_after),
      min_version(constraints.min_version),
      max_version_exclusive(constraints.max_version_exclusive),
      index_not_after(constraints.index_not_after),
      index_after(constraints.index_after),
      validity_starts_not_after(constraints.validity_starts_not_after),
      validity_starts_after(constraints.validity_starts_after) {
  for (std::string_view name : constraints.permitted_dns_names) {
    permitted_dns_names.emplace_back(name);
  }
  if (min_version) {
    CHECK(min_version->IsValid());
  }
  if (max_version_exclusive) {
    CHECK(max_version_exclusive->IsValid());
  }
}

ChromeRootCertConstraints::~ChromeRootCertConstraints() = default;
ChromeRootCertConstraints::ChromeRootCertConstraints(
    const ChromeRootCertConstraints& other) = default;
ChromeRootCertConstraints::ChromeRootCertConstraints(
    ChromeRootCertConstraints&& other) = default;
ChromeRootCertConstraints& ChromeRootCertConstraints::operator=(
    const ChromeRootCertConstraints& other) = default;
ChromeRootCertConstraints& ChromeRootCertConstraints::operator=(
    ChromeRootCertConstraints&& other) = default;

ChromeRootStoreData::Anchor::Anchor(
    std::shared_ptr<const bssl::ParsedCertificate> certificate,
    std::vector<ChromeRootCertConstraints> constraints)
    : ChromeRootStoreData::Anchor::Anchor(certificate,
                                          constraints,
                                          /*enforce_anchor_expiry=*/false,
                                          /*enforce_anchor_constraints=*/false,
                                          /*crs_root_id=*/std::nullopt) {}

ChromeRootStoreData::Anchor::Anchor(
    std::shared_ptr<const bssl::ParsedCertificate> certificate,
    std::vector<ChromeRootCertConstraints> constraints,
    bool enforce_anchor_expiry,
    bool enforce_anchor_constraints,
    std::optional<int32_t> crs_root_id)
    : certificate(std::move(certificate)),
      constraints(std::move(constraints)),
      enforce_anchor_expiry(enforce_anchor_expiry),
      enforce_anchor_constraints(enforce_anchor_constraints),
      crs_root_id(crs_root_id) {}
ChromeRootStoreData::Anchor::~Anchor() = default;

ChromeRootStoreData::Anchor::Anchor(const Anchor& other) = default;
ChromeRootStoreData::Anchor::Anchor(Anchor&& other) = default;
ChromeRootStoreData::Anchor& ChromeRootStoreData::Anchor::operator=(
    const ChromeRootStoreData::Anchor& other) = default;
ChromeRootStoreData::Anchor& ChromeRootStoreData::Anchor::operator=(
    ChromeRootStoreData::Anchor&& other) = default;

ChromeRootStoreData::ChromeRootStoreData() = default;
ChromeRootStoreData::~ChromeRootStoreData() = default;

ChromeRootStoreData::ChromeRootStoreData(const ChromeRootStoreData& other) =
    default;
ChromeRootStoreData::ChromeRootStoreData(ChromeRootStoreData&& other) = default;
ChromeRootStoreData& ChromeRootStoreData::operator=(
    const ChromeRootStoreData& other) = default;
ChromeRootStoreData& ChromeRootStoreData::operator=(
    ChromeRootStoreData&& other) = default;

namespace {

std::optional<std::vector<ChromeRootCertConstraints>> CreateConstraints(
    const ::google::protobuf::RepeatedPtrField<
        ::chrome_root_store::ConstraintSet>& proto_constraints) {
  std::vector<ChromeRootCertConstraints> constraints;
  for (const auto& constraint : proto_constraints) {
    std::optional<base::Version> min_version;
    if (constraint.has_min_version()) {
      min_version = base::Version(constraint.min_version());
      if (!min_version->IsValid()) {
        LOG(ERROR) << "Error parsing version";
        return std::nullopt;
      }
    }

    std::optional<base::Version> max_version_exclusive;
    if (constraint.has_max_version_exclusive()) {
      max_version_exclusive = base::Version(constraint.max_version_exclusive());
      if (!max_version_exclusive->IsValid()) {
        LOG(ERROR) << "Error parsing version";
        return std::nullopt;
      }
    }

    constraints.emplace_back(
        constraint.has_sct_not_after_sec()
            ? std::optional(base::Time::UnixEpoch() +
                            base::Seconds(constraint.sct_not_after_sec()))
            : std::nullopt,
        constraint.has_sct_all_after_sec()
            ? std::optional(base::Time::UnixEpoch() +
                            base::Seconds(constraint.sct_all_after_sec()))
            : std::nullopt,
        min_version, max_version_exclusive,
        base::ToVector(constraint.permitted_dns_names()),
        constraint.has_index_not_after()
            ? std::optional(constraint.index_not_after())
            : std::nullopt,
        constraint.has_index_after() ? std::optional(constraint.index_after())
                                     : std::nullopt,
        constraint.has_validity_starts_not_after_sec()
            ? std::optional(
                  base::Time::UnixEpoch() +
                  base::Seconds(constraint.validity_starts_not_after_sec()))
            : std::nullopt,
        constraint.has_validity_starts_after_sec()
            ? std::optional(
                  base::Time::UnixEpoch() +
                  base::Seconds(constraint.validity_starts_after_sec()))
            : std::nullopt

    );
  }

  return constraints;
}

std::optional<ChromeRootStoreData::Anchor> CreateChromeRootStoreDataAnchor(
    const chrome_root_store::TrustAnchor& anchor) {
  if (anchor.der().empty()) {
    LOG(ERROR) << "Error anchor with empty DER in update";
    return std::nullopt;
  }

  auto parsed = bssl::ParsedCertificate::Create(
      net::x509_util::CreateCryptoBuffer(anchor.der()),
      net::x509_util::DefaultParseCertificateOptions(), nullptr);
  if (!parsed) {
    LOG(ERROR) << "Error parsing cert for update";
    return std::nullopt;
  }

  std::optional<std::vector<ChromeRootCertConstraints>> constraints =
      CreateConstraints(anchor.constraints());
  if (!constraints) {
    return std::nullopt;
  }

  return ChromeRootStoreData::Anchor(
      std::move(parsed), *std::move(constraints),
      anchor.enforce_anchor_expiry(), anchor.enforce_anchor_constraints(),
      anchor.has_crs_root_id() ? std::make_optional(anchor.crs_root_id())
                               : std::nullopt);
}

}  // namespace

std::optional<ChromeRootStoreData>
ChromeRootStoreData::CreateFromRootStoreProto(
    const chrome_root_store::RootStore& proto) {
  ChromeRootStoreData root_store_data;

  for (const auto& anchor : proto.trust_anchors()) {
    // |trust_anchors| are not supposed to have the |tls_trust_anchor| field
    // set, since they are TLS trust anchors definitionally.
    CHECK(!anchor.has_tls_trust_anchor());
    std::optional<ChromeRootStoreData::Anchor> chrome_root_store_data_anchor =
        CreateChromeRootStoreDataAnchor(anchor);
    if (!chrome_root_store_data_anchor) {
      return std::nullopt;
    }
    if (anchor.eutl()) {
      root_store_data.eutl_certs_.emplace_back(
          chrome_root_store_data_anchor.value());
    }
    root_store_data.trust_anchors_.emplace_back(
        std::move(chrome_root_store_data_anchor.value()));
  }

  std::vector<ChromeRootStoreData::Anchor> additional_certs;
  for (const auto& anchor : proto.additional_certs()) {
    std::optional<ChromeRootStoreData::Anchor> chrome_root_store_data_anchor =
        CreateChromeRootStoreDataAnchor(anchor);
    if (!chrome_root_store_data_anchor) {
      return std::nullopt;
    }
    if (anchor.eutl()) {
      root_store_data.eutl_certs_.emplace_back(
          chrome_root_store_data_anchor.value());
    }
    if (anchor.tls_trust_anchor()) {
      root_store_data.trust_anchors_.emplace_back(
          std::move(chrome_root_store_data_anchor.value()));
    }
  }

  root_store_data.version_ = proto.version_major();

  return root_store_data;
}

ChromeRootStoreData ChromeRootStoreData::CreateFromCompiledRootStore() {
  ChromeRootStoreData root_store_data(
      kChromeRootCertList, kEutlRootCertList,
      /*certs_are_static=*/true,
      /*version=*/CompiledChromeRootStoreVersion());
  if (base::FeatureList::IsEnabled(features::kVerifyMTCs)) {
    root_store_data.signer_set_ =
        ChromeRootStoreSignerSet::CreateFromCompiled();
  }
  return root_store_data;
}

ChromeRootStoreData ChromeRootStoreData::CreateForTesting(
    base::span<const ChromeRootCertInfo> certs,
    base::span<const base::span<const uint8_t>> eutl_certs,
    int64_t version) {
  return ChromeRootStoreData(certs, eutl_certs,
                             /*certs_are_static=*/false, version);
}

ChromeRootStoreData::ChromeRootStoreData(
    base::span<const ChromeRootCertInfo> certs,
    base::span<const base::span<const uint8_t>> eutl_certs,
    bool certs_are_static,
    int64_t version)
    : version_(version) {
  // TODO(hchao, sleevi): Explore keeping a CRYPTO_BUFFER of just the DER
  // certificate and subject name. This would hopefully save memory compared
  // to keeping the full parsed representation in memory, especially when
  // there are multiple instances of TrustStoreChrome.
  for (const auto& cert_info : certs) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert;
    if (certs_are_static) {
      // TODO(mattm,hchao): Ensure the static data crypto_buffers for the
      // compiled-in roots are kept alive, so that roots from the component
      // updater data will de-dupe against them. This currently works if the
      // new components roots are the same as the compiled in roots, but
      // fails if a component update drops a root and then the next component
      // update readds the root without a restart.
      cert = x509_util::CreateCryptoBufferFromStaticDataUnsafe(
          cert_info.root_cert_der);
    } else {
      cert = x509_util::CreateCryptoBuffer(cert_info.root_cert_der);
    }
    bssl::CertErrors errors;
    auto parsed = bssl::ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    // There should always be a valid cert, because we should be parsing Chrome
    // Root Store static data compiled in.
    CHECK(parsed);
    std::vector<ChromeRootCertConstraints> cert_constraints;
    for (const auto& constraint : cert_info.constraints) {
      cert_constraints.emplace_back(constraint);
    }
    trust_anchors_.emplace_back(std::move(parsed), std::move(cert_constraints),
                                cert_info.enforce_anchor_expiry,
                                cert_info.enforce_anchor_constraints,
                                cert_info.crs_root_id);
  }

  for (const auto& cert_bytes : eutl_certs) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert;
    if (certs_are_static) {
      cert = x509_util::CreateCryptoBufferFromStaticDataUnsafe(cert_bytes);
    } else {
      cert = x509_util::CreateCryptoBuffer(cert_bytes);
    }
    bssl::CertErrors errors;
    auto parsed = bssl::ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    CHECK(parsed);
    // crs_root_id is not populated for eutl certs, since it isn't quite the
    // same thing. If we want to add an eutl usage histogram, we'd need to
    // consider if we want to use crs_root_id for that anyway, or add an
    // alternate id for eutl certs.
    eutl_certs_.emplace_back(std::move(parsed),
                             std::vector<ChromeRootCertConstraints>());
  }
}

TrustStoreChrome::AnchorExtraData::AnchorExtraData() = default;
TrustStoreChrome::AnchorExtraData::~AnchorExtraData() = default;

TrustStoreChrome::AnchorExtraData::AnchorExtraData(
    const TrustStoreChrome::AnchorExtraData& other) = default;
TrustStoreChrome::AnchorExtraData::AnchorExtraData(
    TrustStoreChrome::AnchorExtraData&& other) = default;
TrustStoreChrome::AnchorExtraData& TrustStoreChrome::AnchorExtraData::operator=(
    const TrustStoreChrome::AnchorExtraData& other) = default;
TrustStoreChrome::AnchorExtraData& TrustStoreChrome::AnchorExtraData::operator=(
    TrustStoreChrome::AnchorExtraData&& other) = default;

TrustStoreChrome::MtcAnchorExtraData::MtcAnchorExtraData() = default;
TrustStoreChrome::MtcAnchorExtraData::~MtcAnchorExtraData() = default;

TrustStoreChrome::MtcAnchorExtraData::MtcAnchorExtraData(
    const TrustStoreChrome::MtcAnchorExtraData& other) = default;
TrustStoreChrome::MtcAnchorExtraData::MtcAnchorExtraData(
    TrustStoreChrome::MtcAnchorExtraData&& other) = default;
TrustStoreChrome::MtcAnchorExtraData&
TrustStoreChrome::MtcAnchorExtraData::operator=(
    const TrustStoreChrome::MtcAnchorExtraData& other) = default;
TrustStoreChrome::MtcAnchorExtraData&
TrustStoreChrome::MtcAnchorExtraData::operator=(
    TrustStoreChrome::MtcAnchorExtraData&& other) = default;

TrustStoreChrome::TrustStoreChrome()
    : TrustStoreChrome(ChromeRootStoreData::CreateFromCompiledRootStore(),
                       /*mtc_metadata=*/nullptr,
                       InitializeConstraintsOverrides()) {}

TrustStoreChrome::TrustStoreChrome(
    const ChromeRootStoreData* root_store_data,
    const ChromeRootStoreMtcMetadata* mtc_metadata)
    : TrustStoreChrome(root_store_data
                           ? *root_store_data
                           : ChromeRootStoreData::CreateFromCompiledRootStore(),
                       mtc_metadata,
                       InitializeConstraintsOverrides()) {}

TrustStoreChrome::TrustStoreChrome(
    const ChromeRootStoreData& root_store_data,
    const ChromeRootStoreMtcMetadata* mtc_metadata,
    ConstraintOverrideMap override_constraints)
    : override_constraints_(std::move(override_constraints)) {

  for (const auto& anchor : root_store_data.trust_anchors()) {
    if (anchor.crs_root_id || !anchor.constraints.empty()) {
      TrustStoreChrome::AnchorExtraData trust_store_anchor_data;
      trust_store_anchor_data.crs_root_id = anchor.crs_root_id;
      trust_store_anchor_data.constraints = anchor.constraints;
      anchor_extra_data_[base::as_string_view(anchor.certificate->der_cert())] =
          std::move(trust_store_anchor_data);
    }

    // If the anchor is configured to enforce expiry and/or X.509 constraints,
    // tell BoringSSL to do so via CertificateTrust settings. Expiry and X.509
    // constraints are enforced by BoringSSL, whereas other constraints in
    // ChromeRootStoreConstraints are enforced by Chrome itself.
    bssl::CertificateTrust certificate_trust =
        bssl::CertificateTrust::ForTrustAnchor();
    if (anchor.enforce_anchor_expiry) {
      certificate_trust = certificate_trust.WithEnforceAnchorExpiry();
    }
    if (anchor.enforce_anchor_constraints) {
      certificate_trust = certificate_trust.WithEnforceAnchorConstraints();
    }
    trust_store_.AddCertificate(anchor.certificate, certificate_trust);
  }
  for (const auto& anchor : root_store_data.eutl_certs()) {
    eutl_trust_store_.AddTrustAnchor(anchor.certificate);
  }

  if (root_store_data.signer_set()) {
    for (const auto& issuer : root_store_data.signer_set()->trusted_issuers()) {
      TrustStoreChrome::MtcAnchorExtraData trust_store_anchor_data;
      trust_store_anchor_data.signer_config = issuer;

      std::map<uint16_t, std::vector<bssl::TrustedSubtree>> trusted_subtrees;
      if (mtc_metadata) {
        auto it = mtc_metadata->plants05_anchor_data().find(issuer.base_id);
        if (it != mtc_metadata->plants05_anchor_data().end()) {
          // `mtc_anchor` is a trusted MTC anchor which also has trusted
          // subtrees supplied in the MTC metadata.
          const ChromeRootStoreMtcMetadata::Plants05AnchorData&
              mtc_anchor_data = it->second;

          trusted_subtrees = mtc_anchor_data.trusted_subtrees;
          trust_store_anchor_data.revoked_serials =
              mtc_anchor_data.revoked_serials;
        }
      }

      auto bssl_mtc_anchor = std::make_shared<const bssl::MTCAnchor>(
          issuer.base_id, issuer.signature_algorithm,
          bssl::UpRef(issuer.key.get()), std::move(trusted_subtrees));
      CHECK(trust_store_.AddMTCTrustAnchor(std::move(bssl_mtc_anchor)));

      mtc_anchor_extra_data_[issuer.base_id] =
          std::move(trust_store_anchor_data);
    }

    signer_set_timestamp_ = root_store_data.signer_set()->timestamp();
    for (const auto& signer : root_store_data.signer_set()->trusted_mirrors()) {
      signer_set_mirrors_[signer.base_id] = signer;
    }
  }

  disable_mtc_mirroring_requirements_ =
      root_store_data.disable_mtc_mirroring_requirements();

  if (mtc_metadata) {
    mtc_metadata_update_time_ = mtc_metadata->update_time();
  }

  version_ = root_store_data.version();
}

TrustStoreChrome::~TrustStoreChrome() = default;

TrustStoreChrome::ConstraintOverrideMap
TrustStoreChrome::InitializeConstraintsOverrides() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestCrsConstraintsSwitch)) {
    return ParseCrsConstraintsSwitch(
        command_line->GetSwitchValueASCII(kTestCrsConstraintsSwitch));
  }

  return {};
}

TrustStoreChrome::ConstraintOverrideMap
TrustStoreChrome::ParseCrsConstraintsSwitch(std::string_view switch_value) {
  // This function constructs a flat_map on the fly rather than the more
  // efficient approach of creating a vector first and then constructing the
  // flat_map from that. It is expected that there will only be a small number
  // of elements in the map, and that this is only used for testing, therefore
  // simplicity of the implementation is weighted higher than theoretical
  // efficiency.
  ConstraintOverrideMap constraints;

  base::StringPairs roots_and_constraints_pairs;
  base::SplitStringIntoKeyValuePairs(switch_value, ':', '+',
                                     &roots_and_constraints_pairs);
  for (const auto& [root_hashes_hex, root_constraints] :
       roots_and_constraints_pairs) {
    std::vector<std::array<uint8_t, crypto::kSHA256Length>> root_hashes;
    for (std::string_view root_hash_hex :
         base::SplitStringPiece(root_hashes_hex, ",", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      std::array<uint8_t, crypto::kSHA256Length> root_hash;
      if (!base::HexStringToSpan(root_hash_hex, root_hash)) {
        LOG(ERROR) << "invalid root hash: " << root_hash_hex;
        continue;
      }
      root_hashes.push_back(std::move(root_hash));
    }
    if (root_hashes.empty()) {
      LOG(ERROR) << "skipped constraintset with no valid root hashes";
      continue;
    }
    ChromeRootCertConstraints constraint;
    base::StringPairs constraint_value_pairs;
    base::SplitStringIntoKeyValuePairs(root_constraints, '=', ',',
                                       &constraint_value_pairs);
    for (const auto& [constraint_name, constraint_value] :
         constraint_value_pairs) {
      std::string constraint_name_lower = base::ToLowerASCII(constraint_name);
      if (constraint_name_lower == "sctnotafter") {
        int64_t value;
        if (!base::StringToInt64(constraint_value, &value)) {
          LOG(ERROR) << "invalid sctnotafter: " << constraint_value;
          continue;
        }
        constraint.sct_not_after =
            base::Time::UnixEpoch() + base::Seconds(value);
      } else if (constraint_name_lower == "sctallafter") {
        int64_t value;
        if (!base::StringToInt64(constraint_value, &value)) {
          LOG(ERROR) << "invalid sctallafter: " << constraint_value;
          continue;
        }
        constraint.sct_all_after =
            base::Time::UnixEpoch() + base::Seconds(value);
      } else if (constraint_name_lower == "minversion") {
        base::Version version(constraint_value);
        if (!version.IsValid()) {
          LOG(ERROR) << "invalid minversion: " << constraint_value;
          continue;
        }
        constraint.min_version = version;
      } else if (constraint_name_lower == "maxversionexclusive") {
        base::Version version(constraint_value);
        if (!version.IsValid()) {
          LOG(ERROR) << "invalid maxversionexclusive: " << constraint_value;
          continue;
        }
        constraint.max_version_exclusive = version;
      } else if (constraint_name_lower == "dns") {
        constraint.permitted_dns_names.push_back(constraint_value);
      } else {
        LOG(ERROR) << "unrecognized constraint " << constraint_name_lower;
      }
    }
    for (const auto& root_hash : root_hashes) {
      constraints[root_hash].push_back(constraint);
    }
  }

  return constraints;
}

void TrustStoreChrome::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                        bssl::ParsedCertificateList* issuers) {
  trust_store_.SyncGetIssuersOf(cert, issuers);
}

bssl::CertificateTrust TrustStoreChrome::GetTrust(
    const bssl::ParsedCertificate* cert) {
  return trust_store_.GetTrust(cert);
}

std::shared_ptr<const bssl::MTCAnchor> TrustStoreChrome::GetTrustedMTCIssuerOf(
    const bssl::ParsedCertificate* cert) {
  return trust_store_.GetTrustedMTCIssuerOf(cert);
}

bool TrustStoreChrome::Contains(const bssl::ParsedCertificate* cert) const {
  return trust_store_.Contains(cert);
}

bool TrustStoreChrome::ContainsMTCAnchor(const bssl::MTCAnchor* anchor) const {
  return trust_store_.ContainsMTCAnchor(anchor);
}

std::optional<int32_t> TrustStoreChrome::GetCrsRootIdForMTC(
    const bssl::MTCAnchor* mtc_anchor) const {
  const MtcAnchorExtraData* anchor_data = GetMTCAnchorData(mtc_anchor->ca_id());
  if (!anchor_data) {
    return {};
  }

  return anchor_data->signer_config.crs_root_id;
}

std::optional<int32_t> TrustStoreChrome::GetCrsRootIdForClassicalCert(
    const bssl::ParsedCertificate* cert) const {
  const AnchorExtraData* anchor_data = GetAnchorData(cert);
  if (!anchor_data) {
    return {};
  }

  return anchor_data->crs_root_id;
}

std::optional<int32_t> TrustStoreChrome::GetCrsRootIdForCert(
    const bssl::CertPathBuilderResultPath* path) const {
  if (std::shared_ptr<const bssl::MTCAnchor> mtc_anchor =
          path->trust_anchor.MTCAnchor();
      mtc_anchor) {
    return GetCrsRootIdForMTC(mtc_anchor.get());
  } else {
    return GetCrsRootIdForClassicalCert(path->certs.back().get());
  }
}

base::span<const ChromeRootCertConstraints>
TrustStoreChrome::GetConstraintsForMTC(
    const bssl::MTCAnchor* mtc_anchor) const {
  const MtcAnchorExtraData* anchor_data = GetMTCAnchorData(mtc_anchor->ca_id());
  if (!anchor_data) {
    return {};
  }

  // TODO(crbug.com/452986180): support constraint overrides for MTC anchors.

  return anchor_data->signer_config.constraints;
}

base::span<const ChromeRootCertConstraints>
TrustStoreChrome::GetConstraintsForClassicalCert(
    const bssl::ParsedCertificate* cert) const {
  if (!override_constraints_.empty()) {
    const std::array<uint8_t, crypto::kSHA256Length> cert_hash =
        crypto::SHA256Hash(cert->der_cert());
    auto it = override_constraints_.find(cert_hash);
    if (it != override_constraints_.end()) {
      return it->second;
    }
  }

  const AnchorExtraData* anchor_data = GetAnchorData(cert);
  if (anchor_data) {
    return anchor_data->constraints;
  }
  return {};
}

base::span<const ChromeRootCertConstraints>
TrustStoreChrome::GetConstraintsForCert(
    const bssl::CertPathBuilderResultPath* path) const {
  if (std::shared_ptr<const bssl::MTCAnchor> mtc_anchor =
          path->trust_anchor.MTCAnchor();
      mtc_anchor) {
    return GetConstraintsForMTC(mtc_anchor.get());
  } else {
    return GetConstraintsForClassicalCert(path->certs.back().get());
  }
}

const TrustStoreChrome::AnchorExtraData* TrustStoreChrome::GetAnchorData(
    const bssl::ParsedCertificate* cert) const {
  auto it = anchor_extra_data_.find(base::as_string_view(cert->der_cert()));
  if (it == anchor_extra_data_.end()) {
    return nullptr;
  }
  return &it->second;
}

const TrustStoreChrome::MtcAnchorExtraData* TrustStoreChrome::GetMTCAnchorData(
    base::span<const uint8_t> log_id) const {
  auto it = mtc_anchor_extra_data_.find(log_id);
  if (it == mtc_anchor_extra_data_.end()) {
    return nullptr;
  }
  return &it->second;
}

// static
std::unique_ptr<TrustStoreChrome> TrustStoreChrome::CreateTrustStoreForTesting(
    base::span<const ChromeRootCertInfo> certs,
    base::span<const base::span<const uint8_t>> eutl_certs,
    int64_t version,
    ConstraintOverrideMap override_constraints) {
  // Note: wrap_unique is used because the constructor is private.
  return base::WrapUnique(new TrustStoreChrome(
      ChromeRootStoreData::CreateForTesting(certs, eutl_certs, version),
      /*mtc_metadata=*/nullptr, std::move(override_constraints)));
}

// static
std::vector<std::vector<uint8_t>>
TrustStoreChrome::GetTrustAnchorIDsFromCompiledInRootStore(
    base::span<const ChromeRootCertInfo> cert_list_for_testing) {
  // TODO(crbug.com/465497426): This method should check the version
  // constraints and not advertise Trust Anchor IDs for anchors that can't work
  // on the running chrome version.
  std::vector<std::vector<uint8_t>> trust_anchor_ids;
  for (const auto& anchor :
       (cert_list_for_testing.empty() ? kChromeRootCertList
                                      : cert_list_for_testing)) {
    if (!anchor.trust_anchor_id.empty()) {
      trust_anchor_ids.emplace_back(base::ToVector(anchor.trust_anchor_id));
    }
  }
  return trust_anchor_ids;
}

// static
std::vector<std::vector<uint8_t>>
TrustStoreChrome::GetTrustedMtcCaIDsFromCompiledInRootStore() {
  return GetTrustedMtcCaIDsFromCompiledInRootStore(
      ChromeRootStoreSignerSet::CreateFromCompiled());
}

// static
std::vector<std::vector<uint8_t>>
TrustStoreChrome::GetTrustedMtcCaIDsFromCompiledInRootStoreForTesting(
    const ChromeRootStoreSignerSet& signer_set) {
  return GetTrustedMtcCaIDsFromCompiledInRootStore(signer_set);
}

// static
std::vector<std::vector<uint8_t>>
TrustStoreChrome::GetTrustedMtcCaIDsFromCompiledInRootStore(
    const ChromeRootStoreSignerSet& signer_set) {
  // TODO(crbug.com/465497426): This method should check the version
  // constraints and not include log IDs for anchors that can't work
  // on the running chrome version. Or that could be done when loading the
  // SignerSet proto.

  std::vector<std::vector<uint8_t>> ca_ids;
  for (const auto& issuer : signer_set.trusted_issuers()) {
    ca_ids.emplace_back(issuer.base_id);
  }
  return ca_ids;
}

std::optional<bssl::VerifyCertificateChainDelegate::MTCCosigner>
TrustStoreChrome::GetMtcMirrorKey(base::span<const uint8_t> cosigner_id) const {
  auto it = signer_set_mirrors_.find(cosigner_id);
  if (it == signer_set_mirrors_.end()) {
    return std::nullopt;
  }
  return bssl::VerifyCertificateChainDelegate::MTCCosigner{
      it->second.signature_algorithm, bssl::UpRef(it->second.key.get())};
}

namespace {
std::string GetOperatorForSignerIfUsableAtTime(const Signer& signer,
                                               base::Time timestamp) {
  bool valid_state = false;
  for (const auto& state_entry : signer.state_history) {
    if (timestamp >= state_entry.state_start) {
      if (state_entry.state == chrome_root_store::STATE_QUALIFIED ||
          state_entry.state == chrome_root_store::STATE_USABLE) {
        // Signer is usable at `timestamp`, fall through to looking at operator
        // history.
        valid_state = true;
        break;
      }
      // Found the state history entry that matches `timestamp`, but the state
      // at that time was not usable. Return failure.
      return {};
    }
  }
  if (!valid_state) {
    return {};
  }

  for (const auto& operator_entry : signer.operator_history) {
    if (timestamp >= operator_entry.operator_start) {
      return operator_entry.name;
    }
  }

  return {};
}

// TODO(crbug.com/452983502): max age from CT policy. Is it good here too?
constexpr base::TimeDelta kMaxSignerSetAge = base::Days(70);
}  // namespace

bool TrustStoreChrome::IsMtcCosignerPolicySatisfied(
    const bssl::ParsedCertificate& target_cert,
    base::Time current_time,
    const bssl::MTCAnchor* mtc_anchor,
    base::span<const std::vector<uint8_t>> valid_additional_cosigners) const {
  if (disable_mtc_mirroring_requirements_) {
    return true;
  }

  if (current_time - signer_set_timestamp_ > kMaxSignerSetAge) {
    // Fail open on old component data.
    return true;
  }

  // policy:  from cqrp draft 0.2.0:
  // Standalone certificates MUST have at least two cosignatures. One of these
  // MUST be from the MTC CA Operator, and one MUST be from a Mirroring Cosigner
  // recognized by Chrome and not operated by the MTC CA Operator.

  // Evaluate operator and state changes relative to the cert notBefore.
  // This isn't ideal but there is no obviously best solution here.
  // TODO(crbug.com/452983502): revisit this?
  base::Time cert_not_before;
  if (!GeneralizedTimeToTime(target_cert.tbs().validity_not_before,
                             &cert_not_before)) {
    return false;
  }

  const TrustStoreChrome::MtcAnchorExtraData* mtc_anchor_data =
      GetMTCAnchorData(mtc_anchor->ca_id());
  if (!mtc_anchor_data) {
    return false;
  }
  const Signer& ca_signer = mtc_anchor_data->signer_config;

  std::string ca_operator =
      GetOperatorForSignerIfUsableAtTime(ca_signer, cert_not_before);
  if (ca_operator.empty()) {
    return false;
  }

  for (const auto& cosigner_id : valid_additional_cosigners) {
    auto it = signer_set_mirrors_.find(cosigner_id);
    if (it == signer_set_mirrors_.end()) {
      continue;
    }
    const Signer& mirror = it->second;
    std::string mirror_operator =
        GetOperatorForSignerIfUsableAtTime(mirror, cert_not_before);
    if (mirror_operator.empty()) {
      continue;
    }

    if (mirror_operator != ca_operator) {
      // Found a mirror that satisfies the policy requirements.
      return true;
    }
  }

  return false;
}

int64_t CompiledChromeRootStoreVersion() {
  return kRootStoreVersion;
}

int64_t CompiledSignerSetTimestampSeconds() {
  return kSignerSetCompiledTimestampSeconds;
}

namespace {

std::optional<ChromeRootStoreMtcMetadata::MtcAnchorData>
CreateMtcAnchorDataForExperiment(
    const chrome_root_store::MtcAnchorData& proto_mtc_anchor_data) {
  if (!proto_mtc_anchor_data.has_log_id() ||
      proto_mtc_anchor_data.log_id().empty() ||
      !proto_mtc_anchor_data.has_trusted_landmark_ids_range() ||
      !proto_mtc_anchor_data.trusted_landmark_ids_range().has_base_id() ||
      !proto_mtc_anchor_data.trusted_landmark_ids_range()
           .has_min_active_landmark_inclusive() ||
      !proto_mtc_anchor_data.trusted_landmark_ids_range()
           .has_last_landmark_inclusive() ||
      proto_mtc_anchor_data.trusted_subtrees_size() == 0) {
    return std::nullopt;
  }

  ChromeRootStoreMtcMetadata::MtcAnchorData mtc_anchor_data;
  mtc_anchor_data.log_id =
      base::ToVector(base::as_byte_span(proto_mtc_anchor_data.log_id()));

  mtc_anchor_data.landmark_base_id = base::ToVector(base::as_byte_span(
      proto_mtc_anchor_data.trusted_landmark_ids_range().base_id()));
  mtc_anchor_data.landmark_min_inclusive =
      proto_mtc_anchor_data.trusted_landmark_ids_range()
          .min_active_landmark_inclusive();
  mtc_anchor_data.landmark_max_inclusive =
      proto_mtc_anchor_data.trusted_landmark_ids_range()
          .last_landmark_inclusive();

  for (const auto& subtree : proto_mtc_anchor_data.trusted_subtrees()) {
    if (!subtree.has_start_inclusive() || !subtree.has_end_exclusive() ||
        !subtree.has_hash() || subtree.hash().size() != crypto::kSHA256Length) {
      return std::nullopt;
    }
    bssl::TrustedSubtree trusted_subtree;
    trusted_subtree.range.start = subtree.start_inclusive();
    trusted_subtree.range.end = subtree.end_exclusive();
    base::span(trusted_subtree.hash)
        .copy_from(base::as_byte_span(subtree.hash()));
    mtc_anchor_data.trusted_subtrees.push_back(std::move(trusted_subtree));
  }

  std::vector<std::pair<uint64_t, uint64_t>> revoked_indices_storage;
  revoked_indices_storage.reserve(proto_mtc_anchor_data.revoked_indices_size());
  for (const auto& revoked_range : proto_mtc_anchor_data.revoked_indices()) {
    if (!revoked_range.has_end_exclusive() ||
        !revoked_range.has_start_inclusive()) {
      return std::nullopt;
    }
    revoked_indices_storage.emplace_back(revoked_range.end_exclusive(),
                                         revoked_range.start_inclusive());
  }
  mtc_anchor_data.revoked_indices =
      base::flat_map<uint64_t, uint64_t>(std::move(revoked_indices_storage));

  return mtc_anchor_data;
}

base::Time ProtoTimestampToTime(const chrome_root_store::Timestamp& timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

base::TimeDelta ProtoDurationToTimeDelta(
    const chrome_root_store::Duration& duration) {
  return base::Seconds(duration.seconds()) +
         base::Nanoseconds(duration.nanos());
}

std::optional<std::vector<uint8_t>> RelativeOidBytesFromText(
    std::string_view oid_text) {
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 32) ||
      !CBB_add_asn1_relative_oid_from_text(cbb.get(), oid_text.data(),
                                           oid_text.size()) ||
      !CBB_flush(cbb.get())) {
    return std::nullopt;
  }
  // SAFETY: CBB_len is guaranteed to return the proper size for CBB_data.
  return base::ToVector(
      UNSAFE_BUFFERS(base::span(CBB_data(cbb.get()), CBB_len(cbb.get()))));
}

// Returns false if `signer` can never be usable in the current configuration,
// and thus is safe to drop completely.
bool IsSignerTrustedAndUsable(const chrome_root_store::Signer& signer) {
  if (!(signer.realm() == chrome_root_store::REALM_PUBLICLY_TRUSTED ||
        (signer.realm() == chrome_root_store::REALM_UNTRUSTED_VALIDATION_ONLY &&
         base::FeatureList::IsEnabled(features::kTestRootStore)))) {
    return false;
  }
  if (signer.state_history().empty()) {
    return false;
  }
  auto latest_state = signer.state_history(0).state();
  if (latest_state != chrome_root_store::STATE_QUALIFIED &&
      latest_state != chrome_root_store::STATE_USABLE &&
      latest_state != chrome_root_store::STATE_FROZEN) {
    return false;
  }
  return true;
}

// Parses the `signer_proto` into a `Signer` object, and if it is trusted and
// usable, adds it to `out_signers`. Returns false if parsing failed.
bool ParseAndFilterSigner(const chrome_root_store::Signer& signer_proto,
                          std::vector<Signer>& out_signers) {
  Signer signer;
  signer.friendly_name = signer_proto.friendly_name();

  std::optional<std::vector<uint8_t>> oid_bytes =
      RelativeOidBytesFromText(signer_proto.base_id());
  if (!oid_bytes.has_value()) {
    return false;
  }
  signer.base_id = std::move(*oid_bytes);

  if (signer_proto.state_history().empty()) {
    return false;
  }
  for (const auto& state : signer_proto.state_history()) {
    if (!state.has_state_start()) {
      return false;
    }
    signer.state_history.emplace_back(
        state.state(), ProtoTimestampToTime(state.state_start()));
  }

  if (signer_proto.operator_history().empty()) {
    return false;
  }
  for (const auto& op : signer_proto.operator_history()) {
    if (!op.has_operator_start()) {
      return false;
    }
    signer.operator_history.emplace_back(
        std::string(op.name()), ProtoTimestampToTime(op.operator_start()));
  }

  signer.type = signer_proto.type();
  signer.realm = signer_proto.realm();

  if (signer_proto.has_max_cert_lifetime()) {
    signer.max_cert_lifetime =
        ProtoDurationToTimeDelta(signer_proto.max_cert_lifetime());
  }

  std::optional<std::vector<ChromeRootCertConstraints>> constraints =
      CreateConstraints(signer_proto.constraints());
  if (!constraints) {
    return false;
  }
  signer.constraints = std::move(*constraints);

  if (signer_proto.has_crs_root_id()) {
    signer.crs_root_id = signer_proto.crs_root_id();
  }
  signer.min_log_number = signer_proto.min_log_number();

  // For component updates, key bytes may be included directly in the proto.
  // We parse them into a bssl::UniquePtr<CRYPTO_BUFFER>.
  // We check if the key matches any compiled-in key from kSignerKeys (defined
  // in signer-set-inc.cc). If it matches, we use the compiled-in span
  // directly without copying. Otherwise, we store a copy in owned_keys_ and
  // reference it.
  if (!signer_proto.key().empty()) {
    auto sha256_hash =
        crypto::SHA256Hash(base::as_byte_span(signer_proto.key()));
    auto it = kSignerKeys.find(base::span<const uint8_t>(sha256_hash));
    if (it != kSignerKeys.end()) {
      // This is safe since this is a key that's compiled in and static.
      signer.key =
          x509_util::CreateCryptoBufferFromStaticDataUnsafe(it->second);
    } else {
      signer.key =
          x509_util::CreateCryptoBuffer(base::as_byte_span(signer_proto.key()));
    }
  } else {
    // For the compiled-in list, the proto in signer-set-inc.cc does not
    // include key bytes, only key_sha256 hashes. Here we look up those hashes
    // in the separate array of key spans (kSignerKeys) and assign the span.
    std::array<uint8_t, crypto::kSHA256Length> sha256_hash;
    if (!base::HexStringToSpan(signer_proto.key_sha256(), sha256_hash)) {
      LOG(ERROR) << "Failed to decode key_sha256 hex: "
                 << signer_proto.key_sha256();
      return false;
    }
    auto it = kSignerKeys.find(base::span<const uint8_t>(sha256_hash));
    if (it == kSignerKeys.end()) {
      LOG(ERROR) << "Could not find key for key_sha256: "
                 << signer_proto.key_sha256();
      return false;
    }
    // This is safe since this is a key that's compiled in and static.
    signer.key = x509_util::CreateCryptoBufferFromStaticDataUnsafe(it->second);
  }

  std::optional<bssl::SignatureAlgorithm> sigalg =
      SignerSignatureAlgorithmToBsslSignatureAlgorithm(
          signer_proto.signature_algorithm());
  if (!sigalg) {
    // An unknown signature algorithm causes the signer to be ignored, and is
    // not considered a parsing failure. We may want to add new signature
    // algorithms in the future, and this gives us more flexibility in how to
    // handle that. (We can still cause clients to fail the whole update by
    // bumping compatibility version if we want that behavior.)
    // This is done after all the other parsing is done to ensure that any
    // parsing errors still cause the proto parsing to fail, rather than being
    // ignored if the error was in a signer with an unknown signature
    // algorithm.
    return true;
  }
  signer.signature_algorithm = *sigalg;

  if (!IsSignerTrustedAndUsable(signer_proto)) {
    // If the signer is not trusted or is retired, don't save it in the output
    // list. Return true to indicate success since it is not an error for the
    // list to contain untrusted signers. This is done after all parsing is
    // done to ensure that any parsing errors still cause the proto parsing to
    // fail, rather than being ignored if the error was in a signer that is
    // filtered out.
    return true;
  }

  out_signers.push_back(std::move(signer));
  return true;
}

std::optional<ChromeRootStoreMtcMetadata::Plants05AnchorData>
CreatePlants05AnchorData(
    const chrome_root_store::MtcAnchorData& proto_mtc_anchor_data) {
  if (!proto_mtc_anchor_data.has_ca_id() ||
      proto_mtc_anchor_data.ca_id().empty()) {
    return std::nullopt;
  }

  std::vector<std::pair<uint64_t, uint64_t>> revoked_indices_storage;
  revoked_indices_storage.reserve(proto_mtc_anchor_data.revoked_indices_size());
  for (const auto& revoked_range : proto_mtc_anchor_data.revoked_indices()) {
    if (!revoked_range.has_end_exclusive() ||
        !revoked_range.has_start_inclusive()) {
      return std::nullopt;
    }
    revoked_indices_storage.emplace_back(revoked_range.end_exclusive(),
                                         revoked_range.start_inclusive());
  }

  ChromeRootStoreMtcMetadata::Plants05AnchorData anchor_data;
  anchor_data.revoked_serials =
      base::flat_map<uint64_t, uint64_t>(std::move(revoked_indices_storage));

  for (const auto& proto_mtc_log_data : proto_mtc_anchor_data.mtc_log_data()) {
    if (!proto_mtc_log_data.has_log_number() ||
        !proto_mtc_log_data.has_trusted_landmark_ids_range() ||
        !proto_mtc_log_data.trusted_landmark_ids_range()
             .has_min_active_landmark_inclusive() ||
        !proto_mtc_log_data.trusted_landmark_ids_range()
             .has_last_landmark_inclusive() ||
        proto_mtc_log_data.trusted_subtrees_size() == 0) {
      return std::nullopt;
    }

    uint16_t log_number = proto_mtc_log_data.log_number();

    ChromeRootStoreMtcMetadata::Plants05AnchorData::LogLandmarkRange
        landmark_range;
    landmark_range.log_number = log_number;
    landmark_range.landmark_min_inclusive =
        proto_mtc_log_data.trusted_landmark_ids_range()
            .min_active_landmark_inclusive();
    landmark_range.landmark_max_inclusive =
        proto_mtc_log_data.trusted_landmark_ids_range()
            .last_landmark_inclusive();
    anchor_data.trusted_landmark_ranges.push_back(landmark_range);

    std::vector<bssl::TrustedSubtree> trusted_subtrees;
    for (const auto& subtree : proto_mtc_log_data.trusted_subtrees()) {
      if (!subtree.has_start_inclusive() || !subtree.has_end_exclusive() ||
          !subtree.has_hash() ||
          subtree.hash().size() != crypto::kSHA256Length) {
        return std::nullopt;
      }
      bssl::TrustedSubtree trusted_subtree;
      trusted_subtree.range.start = subtree.start_inclusive();
      trusted_subtree.range.end = subtree.end_exclusive();
      base::span(trusted_subtree.hash)
          .copy_from(base::as_byte_span(subtree.hash()));
      trusted_subtrees.push_back(std::move(trusted_subtree));
    }
    anchor_data.trusted_subtrees[log_number] = std::move(trusted_subtrees);
  }

  return anchor_data;
}

}  // namespace

ChromeRootStoreMtcMetadata::MtcAnchorData::MtcAnchorData() = default;
ChromeRootStoreMtcMetadata::MtcAnchorData::~MtcAnchorData() = default;

ChromeRootStoreMtcMetadata::MtcAnchorData::MtcAnchorData(
    const ChromeRootStoreMtcMetadata::MtcAnchorData& other) = default;
ChromeRootStoreMtcMetadata::MtcAnchorData::MtcAnchorData(
    ChromeRootStoreMtcMetadata::MtcAnchorData&& other) = default;
ChromeRootStoreMtcMetadata::MtcAnchorData&
ChromeRootStoreMtcMetadata::MtcAnchorData::operator=(
    const ChromeRootStoreMtcMetadata::MtcAnchorData& other) = default;
ChromeRootStoreMtcMetadata::MtcAnchorData&
ChromeRootStoreMtcMetadata::MtcAnchorData::operator=(
    ChromeRootStoreMtcMetadata::MtcAnchorData&& other) = default;

ChromeRootStoreMtcMetadata::Plants05AnchorData::Plants05AnchorData() = default;
ChromeRootStoreMtcMetadata::Plants05AnchorData::~Plants05AnchorData() = default;

ChromeRootStoreMtcMetadata::Plants05AnchorData::Plants05AnchorData(
    const ChromeRootStoreMtcMetadata::Plants05AnchorData& other) = default;
ChromeRootStoreMtcMetadata::Plants05AnchorData::Plants05AnchorData(
    ChromeRootStoreMtcMetadata::Plants05AnchorData&& other) = default;
ChromeRootStoreMtcMetadata::Plants05AnchorData&
ChromeRootStoreMtcMetadata::Plants05AnchorData::operator=(
    const ChromeRootStoreMtcMetadata::Plants05AnchorData& other) = default;
ChromeRootStoreMtcMetadata::Plants05AnchorData&
ChromeRootStoreMtcMetadata::Plants05AnchorData::operator=(
    ChromeRootStoreMtcMetadata::Plants05AnchorData&& other) = default;

ChromeRootStoreMtcMetadata::ChromeRootStoreMtcMetadata() = default;
ChromeRootStoreMtcMetadata::~ChromeRootStoreMtcMetadata() = default;

ChromeRootStoreMtcMetadata::ChromeRootStoreMtcMetadata(
    const ChromeRootStoreMtcMetadata& other) = default;
ChromeRootStoreMtcMetadata::ChromeRootStoreMtcMetadata(
    ChromeRootStoreMtcMetadata&& other) = default;
ChromeRootStoreMtcMetadata& ChromeRootStoreMtcMetadata::operator=(
    const ChromeRootStoreMtcMetadata& other) = default;
ChromeRootStoreMtcMetadata& ChromeRootStoreMtcMetadata::operator=(
    ChromeRootStoreMtcMetadata&& other) = default;

// static
std::optional<ChromeRootStoreMtcMetadata>
ChromeRootStoreMtcMetadata::CreateFromMtcMetadataProto(
    const chrome_root_store::MtcMetadata& proto) {
  ChromeRootStoreMtcMetadata mtc_metadata;

  if (!proto.has_update_time_seconds()) {
    return std::nullopt;
  }
  mtc_metadata.update_time_ =
      base::Time::UnixEpoch() + base::Seconds(proto.update_time_seconds());

  for (const auto& proto_mtc_anchor_data : proto.mtc_anchor_data()) {
    if (proto_mtc_anchor_data.has_ca_id()) {
      std::optional<ChromeRootStoreMtcMetadata::Plants05AnchorData>
          plants05_anchor_data =
              CreatePlants05AnchorData(proto_mtc_anchor_data);
      if (!plants05_anchor_data) {
        return std::nullopt;
      }
      std::vector<uint8_t> ca_id =
          base::ToVector(base::as_byte_span(proto_mtc_anchor_data.ca_id()));
      mtc_metadata.plants05_anchor_data_[ca_id] =
          std::move(plants05_anchor_data).value();
    } else {
      std::optional<ChromeRootStoreMtcMetadata::MtcAnchorData> mtc_anchor_data =
          CreateMtcAnchorDataForExperiment(proto_mtc_anchor_data);
      if (!mtc_anchor_data) {
        return std::nullopt;
      }
      std::vector<uint8_t> log_id = mtc_anchor_data->log_id;
      mtc_metadata.mtc_anchor_data_[log_id] =
          std::move(mtc_anchor_data).value();
    }
  }

  return mtc_metadata;
}

SignerStateChange::SignerStateChange() = default;
SignerStateChange::SignerStateChange(chrome_root_store::SignerState state,
                                     base::Time state_start)
    : state(state), state_start(state_start) {}
SignerStateChange::~SignerStateChange() = default;
SignerStateChange::SignerStateChange(const SignerStateChange& other) = default;
SignerStateChange::SignerStateChange(SignerStateChange&& other) = default;
SignerStateChange& SignerStateChange::operator=(
    const SignerStateChange& other) = default;
SignerStateChange& SignerStateChange::operator=(SignerStateChange&& other) =
    default;

SignerOperatorChange::SignerOperatorChange() = default;
SignerOperatorChange::SignerOperatorChange(std::string name,
                                           base::Time operator_start)
    : name(std::move(name)), operator_start(operator_start) {}
SignerOperatorChange::~SignerOperatorChange() = default;
SignerOperatorChange::SignerOperatorChange(const SignerOperatorChange& other) =
    default;
SignerOperatorChange::SignerOperatorChange(SignerOperatorChange&& other) =
    default;
SignerOperatorChange& SignerOperatorChange::operator=(
    const SignerOperatorChange& other) = default;
SignerOperatorChange& SignerOperatorChange::operator=(
    SignerOperatorChange&& other) = default;

SignerOperator::SignerOperator() = default;
SignerOperator::SignerOperator(std::string name, std::vector<std::string> email)
    : name(std::move(name)), email(std::move(email)) {}
SignerOperator::~SignerOperator() = default;
SignerOperator::SignerOperator(const SignerOperator& other) = default;
SignerOperator::SignerOperator(SignerOperator&& other) = default;
SignerOperator& SignerOperator::operator=(const SignerOperator& other) =
    default;
SignerOperator& SignerOperator::operator=(SignerOperator&& other) = default;

Signer::Signer() = default;
Signer::~Signer() = default;
Signer::Signer(const Signer& other) = default;
Signer::Signer(Signer&& other) = default;
Signer& Signer::operator=(const Signer& other) = default;
Signer& Signer::operator=(Signer&& other) = default;

ChromeRootStoreSignerSet::ChromeRootStoreSignerSet() = default;
ChromeRootStoreSignerSet::~ChromeRootStoreSignerSet() = default;
ChromeRootStoreSignerSet::ChromeRootStoreSignerSet(
    const ChromeRootStoreSignerSet& other) = default;
ChromeRootStoreSignerSet::ChromeRootStoreSignerSet(
    ChromeRootStoreSignerSet&& other) = default;
ChromeRootStoreSignerSet& ChromeRootStoreSignerSet::operator=(
    const ChromeRootStoreSignerSet& other) = default;
ChromeRootStoreSignerSet& ChromeRootStoreSignerSet::operator=(
    ChromeRootStoreSignerSet&& other) = default;

// static
std::optional<ChromeRootStoreSignerSet>
ChromeRootStoreSignerSet::CreateFromProto(
    const chrome_root_store::SignerSet& proto) {
  ChromeRootStoreSignerSet signer_set;

  base::Time timestamp;
  if (proto.has_timestamp()) {
    timestamp = ProtoTimestampToTime(proto.timestamp());
  } else {
    timestamp = base::Time::Min();
  }
  signer_set.timestamp_ = timestamp;
  signer_set.version_ = proto.version();

  for (const auto& op : proto.operators()) {
    std::vector<std::string> emails;
    for (const auto& email : op.email()) {
      emails.emplace_back(email);
    }
    signer_set.operators_.emplace_back(std::string(op.name()),
                                       std::move(emails));
  }

  for (const auto& issuer : proto.issuers()) {
    if (!ParseAndFilterSigner(issuer, signer_set.trusted_issuers_)) {
      return std::nullopt;
    }
  }

  for (const auto& mirror : proto.mirrors()) {
    if (!ParseAndFilterSigner(mirror, signer_set.trusted_mirrors_)) {
      return std::nullopt;
    }
  }

  return signer_set;
}

// static
ChromeRootStoreSignerSet ChromeRootStoreSignerSet::CreateFromCompiled() {
  chrome_root_store::SignerSet proto;
  CHECK(proto.ParseFromArray(kSignerSetProto.data(), kSignerSetProto.size()));

  // The compiled-in proto only contains key_sha256 hashes. When CreateFromProto
  // runs below, it automatically accesses the separate array of key spans
  // (kSignerKeys from signer-set-inc.cc) to populate the key spans for each
  // signer.
  std::optional<ChromeRootStoreSignerSet> signer_set = CreateFromProto(proto);
  CHECK(signer_set.has_value());
  return std::move(*signer_set);
}

}  // namespace net
