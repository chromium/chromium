// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

namespace {
#include "net/data/ssl/chrome_root_store/chrome-root-store-inc.cc"
}  // namespace

ChromeRootCertConstraints::ChromeRootCertConstraints() = default;
ChromeRootCertConstraints::ChromeRootCertConstraints(
    std::optional<base::Time> sct_not_after,
    std::optional<base::Time> sct_all_after,
    std::optional<base::Version> min_version,
    std::optional<base::Version> max_version_exclusive,
    std::vector<std::string> permitted_dns_names)
    : sct_not_after(sct_not_after),
      sct_all_after(sct_all_after),
      min_version(std::move(min_version)),
      max_version_exclusive(std::move(max_version_exclusive)),
      permitted_dns_names(std::move(permitted_dns_names)) {}

ChromeRootCertConstraints::ChromeRootCertConstraints(
    const StaticChromeRootCertConstraints& constraints)
    : sct_not_after(constraints.sct_not_after),
      sct_all_after(constraints.sct_all_after),
      min_version(constraints.min_version),
      max_version_exclusive(constraints.max_version_exclusive) {
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
    : ChromeRootStoreData::Anchor::Anchor(
          certificate,
          constraints,
          /*enforce_anchor_expiry=*/false,
          /*enforce_anchor_constraints=*/false) {}

ChromeRootStoreData::Anchor::Anchor(
    std::shared_ptr<const bssl::ParsedCertificate> certificate,
    std::vector<ChromeRootCertConstraints> constraints,
    bool enforce_anchor_expiry,
    bool enforce_anchor_constraints)
    : certificate(std::move(certificate)),
      constraints(std::move(constraints)),
      enforce_anchor_expiry(enforce_anchor_expiry),
      enforce_anchor_constraints(enforce_anchor_constraints) {}
ChromeRootStoreData::Anchor::~Anchor() = default;

ChromeRootStoreData::Anchor::Anchor(const Anchor& other) = default;
ChromeRootStoreData::Anchor::Anchor(Anchor&& other) = default;
ChromeRootStoreData::Anchor& ChromeRootStoreData::Anchor::operator=(
    const ChromeRootStoreData::Anchor& other) = default;
ChromeRootStoreData::Anchor& ChromeRootStoreData::Anchor::operator=(
    ChromeRootStoreData::Anchor&& other) = default;

ChromeRootStoreData::MtcAnchor::MtcAnchor(
    std::vector<uint8_t> log_id,
    std::vector<ChromeRootCertConstraints> constraints)
    : log_id(std::move(log_id)), constraints(std::move(constraints)) {}
ChromeRootStoreData::MtcAnchor::~MtcAnchor() = default;

ChromeRootStoreData::MtcAnchor::MtcAnchor(const MtcAnchor& other) = default;
ChromeRootStoreData::MtcAnchor::MtcAnchor(MtcAnchor&& other) = default;
ChromeRootStoreData::MtcAnchor& ChromeRootStoreData::MtcAnchor::operator=(
    const ChromeRootStoreData::MtcAnchor& other) = default;
ChromeRootStoreData::MtcAnchor& ChromeRootStoreData::MtcAnchor::operator=(
    ChromeRootStoreData::MtcAnchor&& other) = default;

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
        base::ToVector(constraint.permitted_dns_names()));
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

  return ChromeRootStoreData::Anchor(std::move(parsed), *std::move(constraints),
                                     anchor.enforce_anchor_expiry(),
                                     anchor.enforce_anchor_constraints());
}

std::optional<ChromeRootStoreData::MtcAnchor>
CreateChromeRootStoreDataMtcAnchor(
    const chrome_root_store::MtcAnchor& mtc_anchor) {
  if (!mtc_anchor.has_log_id() || mtc_anchor.log_id().empty()) {
    LOG(ERROR) << "Error MTC anchor with empty log_id in update";
    return std::nullopt;
  }

  std::optional<std::vector<ChromeRootCertConstraints>> constraints =
      CreateConstraints(mtc_anchor.constraints());
  if (!constraints) {
    return std::nullopt;
  }

  return ChromeRootStoreData::MtcAnchor(
      base::ToVector(base::as_byte_span(mtc_anchor.log_id())),
      *std::move(constraints));
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

  if (base::FeatureList::IsEnabled(features::kVerifyMTCs)) {
    for (const auto& mtc_anchor : proto.mtc_anchors()) {
      std::optional<ChromeRootStoreData::MtcAnchor>
          chrome_root_store_data_mtc_anchor =
              CreateChromeRootStoreDataMtcAnchor(mtc_anchor);
      if (!chrome_root_store_data_mtc_anchor) {
        return std::nullopt;
      }
      if (mtc_anchor.tls_trust_anchor()) {
        root_store_data.mtc_trust_anchors_.emplace_back(
            std::move(chrome_root_store_data_mtc_anchor.value()));
      }
    }
  }

  root_store_data.version_ = proto.version_major();

  return root_store_data;
}

ChromeRootStoreData ChromeRootStoreData::CreateFromCompiledRootStore() {
  // TODO(crbug.com/452983502): Populate initial MTC trust anchors from
  // compiled-in data.
  return ChromeRootStoreData(kChromeRootCertList, kEutlRootCertList,
                             /*certs_are_static=*/true,
                             /*version=*/CompiledChromeRootStoreVersion());
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
                                cert_info.enforce_anchor_constraints);
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
    eutl_certs_.emplace_back(std::move(parsed),
                             std::vector<ChromeRootCertConstraints>());
  }
}

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
  std::vector<
      std::pair<std::string_view, std::vector<ChromeRootCertConstraints>>>
      constraints;

  for (const auto& anchor : root_store_data.trust_anchors()) {
    if (!anchor.constraints.empty()) {
      constraints.emplace_back(
          base::as_string_view(anchor.certificate->der_cert()),
          anchor.constraints);
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

  // TODO(crbug.com/452983502): currently mtc anchors are only used with
  // signatureless certificates, so they are ignored if the mtc_metadata is not
  // available yet. Change this once we supported "full" MTCs.
  if (mtc_metadata) {
    for (const auto& mtc_anchor : root_store_data.mtc_trust_anchors()) {
      auto it = mtc_metadata->mtc_anchor_data().find(mtc_anchor.log_id);
      if (it != mtc_metadata->mtc_anchor_data().end()) {
        // `mtc_anchor` is a trusted MTC anchor which also has trusted subtrees
        // supplied in the MTC metadata.
        const ChromeRootStoreMtcMetadata::MtcAnchorData& mtc_anchor_data =
            it->second;

        if (!mtc_anchor.constraints.empty()) {
          // TODO(crbug.com/452986180): MTC anchor constraints aren't handled
          // yet. Ignore any MTC anchors that have constraints until they are
          // implemented, which ensures that if any old versions of chrome
          // still happen to be running and receive a component update with an
          // MTC anchor that has constraints, it will fail-safe.
          continue;
        }

        auto bssl_mtc_anchor = std::make_shared<const bssl::MTCAnchor>(
            mtc_anchor.log_id, mtc_anchor_data.trusted_subtrees);
        CHECK(trust_store_.AddMTCTrustAnchor(std::move(bssl_mtc_anchor)));

        if (!mtc_anchor.constraints.empty()) {
          // TODO(crbug.com/452986180): enforce MTC anchor constraints in the
          // verifier.
          mtc_constraints_[mtc_anchor.log_id] = mtc_anchor.constraints;
        }
      }
    }
  }

  constraints_ = base::flat_map(std::move(constraints));
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
      // TODO(crbug.com/40941039): add other constraint types here when they
      // are implemented
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

base::span<const ChromeRootCertConstraints>
TrustStoreChrome::GetConstraintsForCert(
    const bssl::ParsedCertificate* cert) const {
  if (!override_constraints_.empty()) {
    const std::array<uint8_t, crypto::kSHA256Length> cert_hash =
        crypto::SHA256Hash(cert->der_cert());
    auto it = override_constraints_.find(cert_hash);
    if (it != override_constraints_.end()) {
      return it->second;
    }
  }

  auto it = constraints_.find(base::as_string_view(cert->der_cert()));
  if (it != constraints_.end()) {
    return it->second;
  }
  return {};
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

int64_t CompiledChromeRootStoreVersion() {
  return kRootStoreVersion;
}

namespace {

std::optional<ChromeRootStoreMtcMetadata::MtcAnchorData> CreateMtcAnchorData(
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

  // TODO(crbug.com/452986179): handle revoked_indices too

  return mtc_anchor_data;
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
    std::optional<ChromeRootStoreMtcMetadata::MtcAnchorData> mtc_anchor_data =
        CreateMtcAnchorData(proto_mtc_anchor_data);
    if (!mtc_anchor_data) {
      return std::nullopt;
    }
    std::vector<uint8_t> log_id = mtc_anchor_data->log_id;
    mtc_metadata.mtc_anchor_data_[log_id] = std::move(mtc_anchor_data).value();
  }

  return mtc_metadata;
}

}  // namespace net
