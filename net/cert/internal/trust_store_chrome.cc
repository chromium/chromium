// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"

#include <optional>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
    : TrustStoreChrome(kChromeRootCertList,
                       /*certs_are_static=*/true,
                       /*version=*/CompiledChromeRootStoreVersion()) {}

TrustStoreChrome::TrustStoreChrome(base::span<const ChromeRootCertInfo> certs,
                                   bool certs_are_static,
                                   int64_t version) {
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

TrustStoreChrome::TrustStoreChrome(const ChromeRootStoreData& root_store_data) {
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
  auto it = constraints_.find(cert->der_cert().AsStringView());
  if (it != constraints_.end()) {
    return it->second;
  }
  return {};
}

// static
std::unique_ptr<TrustStoreChrome> TrustStoreChrome::CreateTrustStoreForTesting(
    base::span<const ChromeRootCertInfo> certs,
    int64_t version) {
  // Note: wrap_unique is used because the constructor is private.
  return base::WrapUnique(new TrustStoreChrome(
      certs, /*certs_are_static=*/false, /*version=*/version));
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
