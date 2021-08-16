// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_win.h"

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_util.h"
#include "net/third_party/mozilla_win/cert/win_util.h"

namespace net {

// TODO(https://crbug.com/1239258): import and use distrust settings.
// TODO(https://crbug.com/1239260): limit certs if they have EKU settings.
// TODO(https://crbug.com/1239268): support CTLs.
std::unique_ptr<TrustStoreWin> TrustStoreWin::Create() {
  crypto::ScopedHCERTSTORE root_cert_store(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE intermediate_cert_store(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  crypto::ScopedHCERTSTORE all_certs_store(
      CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, NULL, 0, nullptr));
  if (!root_cert_store.get() || !intermediate_cert_store.get() ||
      !all_certs_store.get()) {
    return nullptr;
  }

  // Grab the user-added roots.
  GatherEnterpriseCertsForLocation(root_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE, L"ROOT");
  GatherEnterpriseCertsForLocation(root_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                                   L"ROOT");
  GatherEnterpriseCertsForLocation(root_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                   L"ROOT");
  GatherEnterpriseCertsForLocation(root_cert_store.get(),
                                   CERT_SYSTEM_STORE_CURRENT_USER, L"ROOT");
  GatherEnterpriseCertsForLocation(root_cert_store.get(),
                                   CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
                                   L"ROOT");

  // Grab the user-added intermediates.
  GatherEnterpriseCertsForLocation(intermediate_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE, L"CA");
  GatherEnterpriseCertsForLocation(intermediate_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                                   L"CA");
  GatherEnterpriseCertsForLocation(intermediate_cert_store.get(),
                                   CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                                   L"CA");
  GatherEnterpriseCertsForLocation(intermediate_cert_store.get(),
                                   CERT_SYSTEM_STORE_CURRENT_USER, L"CA");
  GatherEnterpriseCertsForLocation(intermediate_cert_store.get(),
                                   CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
                                   L"CA");

  CertAddStoreToCollection(all_certs_store.get(), intermediate_cert_store.get(),
                           /*dwUpdateFlags=*/0, /*dwPriority=*/0);

  CertAddStoreToCollection(all_certs_store.get(), root_cert_store.get(),
                           /*dwUpdateFlags=*/0, /*dwPriority=*/0);
  return base::WrapUnique(new TrustStoreWin(std::move(root_cert_store),
                                            std::move(all_certs_store)));
}

std::unique_ptr<TrustStoreWin> TrustStoreWin::CreateForTesting(
    crypto::ScopedHCERTSTORE root_cert_store,
    crypto::ScopedHCERTSTORE all_certs_store) {
  return base::WrapUnique(new TrustStoreWin(std::move(root_cert_store),
                                            std::move(all_certs_store)));
}

TrustStoreWin::TrustStoreWin(crypto::ScopedHCERTSTORE root_cert_store,
                             crypto::ScopedHCERTSTORE all_certs_store)
    : root_cert_store_(std::move(root_cert_store)),
      all_certs_store_(std::move(all_certs_store)) {}

TrustStoreWin::~TrustStoreWin() = default;

void TrustStoreWin::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
  base::span<const uint8_t> issuer_span = cert->issuer_tlv().AsSpan();

  CERT_NAME_BLOB cert_issuer_blob;
  cert_issuer_blob.cbData = static_cast<DWORD>(issuer_span.size());
  cert_issuer_blob.pbData = const_cast<uint8_t*>(issuer_span.data());

  PCCERT_CONTEXT cert_from_store = nullptr;
  // TODO(https://crbug.com/1239270): figure out if this is thread-safe or if we
  // need locking here
  while ((cert_from_store = CertFindCertificateInStore(
              all_certs_store_.get(), X509_ASN_ENCODING, 0,
              CERT_FIND_SUBJECT_NAME, &cert_issuer_blob, cert_from_store))) {
    bssl::UniquePtr<CRYPTO_BUFFER> der_crypto = x509_util::CreateCryptoBuffer(
        cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded);
    CertErrors errors;
    ParsedCertificate::CreateAndAddToVector(
        std::move(der_crypto), x509_util::DefaultParseCertificateOptions(),
        issuers, &errors);
  }
}

void TrustStoreWin::GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                             CertificateTrust* trust,
                             base::SupportsUserData* debug_data) const {
  base::span<const uint8_t> cert_span = cert->der_cert().AsSpan();
  base::SHA1Digest cert_hash = base::SHA1HashSpan(cert_span);
  CRYPT_HASH_BLOB cert_hash_blob;
  cert_hash_blob.cbData = static_cast<DWORD>(cert_hash.size());
  cert_hash_blob.pbData = cert_hash.data();

  PCCERT_CONTEXT cert_from_store = nullptr;

  // TODO(https://crbug.com/1239270): figure out if this is thread-safe or if we
  // need locking here
  while ((cert_from_store = CertFindCertificateInStore(
              root_cert_store_.get(), X509_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH,
              &cert_hash_blob, cert_from_store))) {
    base::span<const uint8_t> cert_from_store_span = base::make_span(
        cert_from_store->pbCertEncoded, cert_from_store->cbCertEncoded);

    if (base::ranges::equal(cert_span, cert_from_store_span)) {
      // Found cert, yay!
      *trust = CertificateTrust::ForTrustAnchor();
      // Free before returning
      CertFreeCertificateContext(cert_from_store);
      return;
    }
  }

  // Didn't find cert, return Unspecified Trust.
  *trust = CertificateTrust::ForUnspecified();
}

}  // namespace net