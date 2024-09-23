// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
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
    std::optional<base::Version> max_version_exclusive)
    : sct_not_after(sct_not_after),
      sct_all_after(sct_all_after),
      min_version(std::move(min_version)),
      max_version_exclusive(std::move(max_version_exclusive)) {}
ChromeRootCertConstraints::ChromeRootCertConstraints(
    const StaticChromeRootCertConstraints& constraints)
    : sct_not_after(constraints.sct_not_after),
      sct_all_after(constraints.sct_all_after),
      min_version(constraints.min_version),
      max_version_exclusive(constraints.max_version_exclusive) {
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
    : certificate(std::move(certificate)),
      constraints(std::move(constraints)) {}
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

std::optional<ChromeRootStoreData>
ChromeRootStoreData::CreateChromeRootStoreData(
    const chrome_root_store::RootStore& proto) {
  ChromeRootStoreData root_store_data;

  for (auto& anchor : proto.trust_anchors()) {
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

    std::vector<ChromeRootCertConstraints> constraints;
    for (const auto& constraint : anchor.constraints()) {
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
        max_version_exclusive =
            base::Version(constraint.max_version_exclusive());
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
          min_version, max_version_exclusive);
    }
    root_store_data.anchors_.emplace_back(std::move(parsed),
                                          std::move(constraints));
  }

  root_store_data.version_ = proto.version_major();

  return root_store_data;
}

TrustStoreChrome::TrustStoreChrome()
    : TrustStoreChrome(
          kChromeRootCertList,
          /*certs_are_static=*/true,
          /*version=*/CompiledChromeRootStoreVersion(),
          /*override_constraints=*/InitializeConstraintsOverrides()) {}

TrustStoreChrome::TrustStoreChrome(base::span<const ChromeRootCertInfo> certs,
                                   bool certs_are_static,
                                   int64_t version,
                                   ConstraintOverrideMap override_constraints)
    : override_constraints_(std::move(override_constraints)) {
  std::vector<
      std::pair<std::string_view, std::vector<ChromeRootCertConstraints>>>
      constraints;

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
    if (!cert_info.constraints.empty()) {
      std::vector<ChromeRootCertConstraints> cert_constraints;
      for (const auto& constraint : cert_info.constraints) {
        cert_constraints.emplace_back(constraint);
      }
      constraints.emplace_back(parsed->der_cert().AsStringView(),
                               std::move(cert_constraints));
    }
    trust_store_.AddTrustAnchor(std::move(parsed));
  }

  constraints_ = base::flat_map(std::move(constraints));
  version_ = version;
}

TrustStoreChrome::TrustStoreChrome(const ChromeRootStoreData& root_store_data)
    : override_constraints_(InitializeConstraintsOverrides()) {
  std::vector<
      std::pair<std::string_view, std::vector<ChromeRootCertConstraints>>>
      constraints;

  for (const auto& anchor : root_store_data.anchors()) {
    if (!anchor.constraints.empty()) {
      constraints.emplace_back(anchor.certificate->der_cert().AsStringView(),
                               anchor.constraints);
    }
    trust_store_.AddTrustAnchor(anchor.certificate);
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

bool TrustStoreChrome::Contains(const bssl::ParsedCertificate* cert) const {
  return trust_store_.Contains(cert);
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

  auto it = constraints_.find(cert->der_cert().AsStringView());
  if (it != constraints_.end()) {
    return it->second;
  }
  return {};
}

// static
std::unique_ptr<TrustStoreChrome> TrustStoreChrome::CreateTrustStoreForTesting(
    base::span<const ChromeRootCertInfo> certs,
    int64_t version,
    ConstraintOverrideMap override_constraints) {
  // Note: wrap_unique is used because the constructor is private.
  return base::WrapUnique(new TrustStoreChrome(
      certs,
      /*certs_are_static=*/false,
      /*version=*/version, std::move(override_constraints)));
}

int64_t CompiledChromeRootStoreVersion() {
  return kRootStoreVersion;
}

std::vector<ChromeRootStoreData::Anchor> CompiledChromeRootStoreAnchors() {
  std::vector<ChromeRootStoreData::Anchor> anchors;
  for (const auto& cert_info : kChromeRootCertList) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert =
        x509_util::CreateCryptoBufferFromStaticDataUnsafe(
            cert_info.root_cert_der);
    bssl::CertErrors errors;
    auto parsed = bssl::ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    DCHECK(parsed);

    std::vector<ChromeRootCertConstraints> cert_constraints;
    for (const auto& constraint : cert_info.constraints) {
      cert_constraints.emplace_back(constraint);
    }
    anchors.emplace_back(std::move(parsed), std::move(cert_constraints));
  }

  return anchors;
}

}  // namespace net
