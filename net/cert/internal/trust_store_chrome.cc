// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_chrome.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {
#include "net/data/ssl/chrome_root_store/chrome-root-store-inc.cc"
}  // namespace

ChromeRootStoreData::ChromeRootStoreData() = default;

ChromeRootStoreData::~ChromeRootStoreData() = default;

ChromeRootStoreData::ChromeRootStoreData(const ChromeRootStoreData& other) =
    default;
ChromeRootStoreData::ChromeRootStoreData(ChromeRootStoreData&& other) = default;
ChromeRootStoreData& ChromeRootStoreData::operator=(
    const ChromeRootStoreData& other) = default;
ChromeRootStoreData& ChromeRootStoreData::operator=(
    ChromeRootStoreData&& other) = default;

absl::optional<ChromeRootStoreData>
ChromeRootStoreData::CreateChromeRootStoreData(
    const chrome_root_store::RootStore& proto) {
  ChromeRootStoreData root_store_data;

  for (auto& anchor : proto.trust_anchors()) {
    if (anchor.der().empty()) {
      LOG(ERROR) << "Error anchor with empty DER in update";
      return absl::nullopt;
    }

    auto parsed = net::ParsedCertificate::Create(
        net::x509_util::CreateCryptoBuffer(anchor.der()),
        net::x509_util::DefaultParseCertificateOptions(), nullptr);
    if (!parsed) {
      LOG(ERROR) << "Error parsing cert for update";
      return absl::nullopt;
    }
    root_store_data.anchors_.push_back(std::move(parsed));
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
    CertErrors errors;
    auto parsed = ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    DCHECK(parsed);
    trust_store_.AddTrustAnchor(parsed);
  }
  version_ = version;
}

TrustStoreChrome::TrustStoreChrome(const ChromeRootStoreData& root_store_data) {
  for (const auto& anchor : root_store_data.anchors()) {
    trust_store_.AddTrustAnchor(anchor);
  }
  version_ = root_store_data.version();
}

TrustStoreChrome::~TrustStoreChrome() = default;

void TrustStoreChrome::SyncGetIssuersOf(const ParsedCertificate* cert,
                                        ParsedCertificateList* issuers) {
  trust_store_.SyncGetIssuersOf(cert, issuers);
}

CertificateTrust TrustStoreChrome::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  return trust_store_.GetTrust(cert, debug_data);
}

bool TrustStoreChrome::Contains(const ParsedCertificate* cert) const {
  return trust_store_.Contains(cert);
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

ParsedCertificateList CompiledChromeRootStoreAnchors() {
  ParsedCertificateList parsed_cert_list;
  for (const auto& cert_info : kChromeRootCertList) {
    bssl::UniquePtr<CRYPTO_BUFFER> cert =
        x509_util::CreateCryptoBufferFromStaticDataUnsafe(
            cert_info.root_cert_der);
    CertErrors errors;
    auto parsed = ParsedCertificate::Create(
        std::move(cert), x509_util::DefaultParseCertificateOptions(), &errors);
    DCHECK(parsed);
    parsed_cert_list.push_back(parsed);
  }

  return parsed_cert_list;
}

}  // namespace net
