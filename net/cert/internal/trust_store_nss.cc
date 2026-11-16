// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <certt.h>
#include <pk11pub.h>
#include <pkcs11n.h>
#include <pkcs11t.h>
#include <seccomon.h>
#include <secmod.h>
#include <secmodt.h>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

namespace {

bool IsMozillaCaPolicyProvided(PK11SlotInfo* slot,
                               CK_OBJECT_HANDLE cert_handle) {
  return PK11_HasRootCerts(slot) &&
         PK11_HasAttributeSet(slot, cert_handle, CKA_NSS_MOZILLA_CA_POLICY,
                              /*haslock=*/PR_FALSE) == CK_TRUE;
}

std::vector<crypto::ScopedPK11Slot> InitializeRootCertSlots() {
  crypto::EnsureNSSInit();

  std::vector<crypto::ScopedPK11Slot> result;
  crypto::AutoSECMODListReadLock lock_id;
  for (const SECMODModuleList* item = SECMOD_GetDefaultModuleList();
       item != nullptr; item = item->next) {
    // SAFETY: item->module->slots is an array with item->module->slotCount
    // elements. slotCount is a signed int so use checked_cast when creating
    // the span.
    base::span<PK11SlotInfo*> module_slots = UNSAFE_BUFFERS(
        base::span(item->module->slots,
                   base::checked_cast<size_t>(item->module->slotCount)));

    for (PK11SlotInfo* slot : module_slots) {
      if (PK11_IsPresent(slot) && PK11_HasRootCerts(slot)) {
        result.emplace_back(PK11_ReferenceSlot(slot));
      }
    }
  }
  return result;
}

const std::vector<crypto::ScopedPK11Slot>& GetRootCertSlots() {
  static base::NoDestructor<std::vector<crypto::ScopedPK11Slot>>
      root_cert_slots(InitializeRootCertSlots());
  return *root_cert_slots.get();
}

bool IsCertInNSSRoots(CERTCertificate* nss_cert) {
  for (const auto& slot : GetRootCertSlots()) {
    if (PK11_IsPresent(slot.get())) {
      CK_OBJECT_HANDLE handle =
          PK11_FindCertInSlot(slot.get(), nss_cert, nullptr);
      if (handle != CK_INVALID_HANDLE) {
        if (IsMozillaCaPolicyProvided(slot.get(), handle)) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace

TrustStoreNSS::ListCertsResult::ListCertsResult(ScopedCERTCertificate cert,
                                                bssl::CertificateTrust trust)
    : cert(std::move(cert)), trust(trust) {}
TrustStoreNSS::ListCertsResult::~ListCertsResult() = default;

TrustStoreNSS::ListCertsResult::ListCertsResult(ListCertsResult&& other) =
    default;
TrustStoreNSS::ListCertsResult& TrustStoreNSS::ListCertsResult::operator=(
    ListCertsResult&& other) = default;

TrustStoreNSS::TrustStoreNSS() = default;
TrustStoreNSS::~TrustStoreNSS() = default;

void TrustStoreNSS::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                     bssl::ParsedCertificateList* issuers) {
  crypto::EnsureNSSInit();

  SECItem name;
  // Use the original issuer value instead of the normalized version. NSS does a
  // less extensive normalization in its Name comparisons, so our normalized
  // version may not match the unnormalized version.
  name.len = cert->tbs().issuer_tlv.size();
  name.data = const_cast<uint8_t*>(cert->tbs().issuer_tlv.data());

  // |validOnly| in CERT_CreateSubjectCertList controls whether to return only
  // certs that are valid at |sorttime|. Expiration isn't meaningful for trust
  // anchors, so request all the matches.
  crypto::ScopedCERTCertList found_certs(CERT_CreateSubjectCertList(
      nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
      PR_Now() /* sorttime */, PR_FALSE /* validOnly */));

  if (!found_certs) {
    return;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
    bssl::CertErrors parse_errors;
    std::shared_ptr<const bssl::ParsedCertificate> cur_cert =
        bssl::ParsedCertificate::Create(
            x509_util::CreateCryptoBuffer(
                x509_util::CERTCertificateAsSpan(node->cert)),
            {}, &parse_errors);

    if (!cur_cert) {
      // TODO(crbug.com/41267838): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << parse_errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(cur_cert));
  }
}

std::vector<TrustStoreNSS::ListCertsResult>
TrustStoreNSS::ListCertsIgnoringNSSRootsImpl() {
  crypto::EnsureNSSInit();
  std::vector<TrustStoreNSS::ListCertsResult> results;
  crypto::ScopedCERTCertList cert_list;
  cert_list.reset(PK11_ListCerts(PK11CertListUnique, nullptr));
  // PK11_ListCerts can return nullptr, e.g. because the PKCS#11 token
  // that was backing the specified slot is not available anymore.
  // Treat it as no certificates being present on the slot.
  if (!cert_list) {
    LOG(WARNING) << "PK11_ListCerts returned null";
    return results;
  }

  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list); !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    if (IsCertInNSSRoots(node->cert)) {
      continue;
    }
    CERTCertTrust nss_cert_trust;
    bssl::CertificateTrust trust =
        CERT_GetCertTrust(node->cert, &nss_cert_trust) == SECSuccess
            ? GetTrustForNSSTrust(nss_cert_trust)
            : bssl::CertificateTrust::ForUnspecified();
    results.emplace_back(x509_util::DupCERTCertificate(node->cert), trust);
  }

  return results;
}

bssl::CertificateTrust TrustStoreNSS::GetTrust(
    const bssl::ParsedCertificate* cert) {
  crypto::EnsureNSSInit();

  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(cert->der_cert().data());
  der_cert.len = base::checked_cast<unsigned>(cert->der_cert().size());
  der_cert.type = siDERCertBuffer;

  // Find a matching NSS certificate object, if any. Note that NSS trust
  // objects can also be keyed on issuer+serial and match any such cert. This
  // is only used for distrust and apparently only in the NSS builtin roots
  // certs module. Therefore, it should be safe to use the more efficient
  // CERT_FindCertByDERCert to avoid having to have NSS parse the certificate
  // and create a structure for it if the cert doesn't already exist in any of
  // the loaded NSS databases.
  ScopedCERTCertificate nss_cert(
      CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));

  if (!nss_cert) {
    DVLOG(1) << "skipped cert that has no CERTCertificate already";
    return bssl::CertificateTrust::ForUnspecified();
  }

  // CERT_FindCertByDERCert may have returned a different cert that has the same
  // issuer+serial. A trust record should only be used if it's really the same
  // cert.
  if (x509_util::CERTCertificateAsSpan(nss_cert.get()) != cert->der_cert()) {
    DVLOG(1) << "skipped non-identical cert returned by CERT_FindCertByDERCert";
    return bssl::CertificateTrust::ForUnspecified();
  }

  return GetTrustIgnoringSystemTrust(nss_cert.get());
}

std::shared_ptr<const bssl::MTCAnchor> TrustStoreNSS::GetTrustedMTCIssuerOf(
    const bssl::ParsedCertificate* cert) {
  return nullptr;
}

bssl::CertificateTrust TrustStoreNSS::GetTrustIgnoringSystemTrust(
    CERTCertificate* nss_cert) const {
  // See if NSS has any trust settings for the certificate at all. If not,
  // there is no point in doing further work.
  CERTCertTrust nss_cert_trust;
  if (CERT_GetCertTrust(nss_cert, &nss_cert_trust) != SECSuccess) {
    DVLOG(1) << "skipped cert that has no trust settings";
    return bssl::CertificateTrust::ForUnspecified();
  }

  if (IsCertInNSSRoots(nss_cert)) {
    DVLOG(1) << "skipped cert that is in NSS roots";
    return bssl::CertificateTrust::ForUnspecified();
  }

  DVLOG(1) << "cert is not in NSS roots, using NSS calculated trust";
  return GetTrustForNSSTrust(nss_cert_trust);
}

bssl::CertificateTrust TrustStoreNSS::GetTrustForNSSTrust(
    const CERTCertTrust& trust) const {
  unsigned int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trustSSL);

  // Determine if the certificate is distrusted.
  if ((trust_flags & (CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED_CA |
                      CERTDB_TRUSTED)) == CERTDB_TERMINAL_RECORD) {
    return bssl::CertificateTrust::ForDistrusted();
  }

  // Determine if the certificate is a trust anchor.
  bool is_trusted_ca = (trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA;

  constexpr unsigned int kTrustedPeerBits =
      CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
  bool is_trusted_leaf = (trust_flags & kTrustedPeerBits) == kTrustedPeerBits;

  if (is_trusted_ca && is_trusted_leaf) {
    return bssl::CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  } else if (is_trusted_ca) {
    return bssl::CertificateTrust::ForTrustAnchor()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  } else if (is_trusted_leaf) {
    return bssl::CertificateTrust::ForTrustedLeaf();
  }

  return bssl::CertificateTrust::ForUnspecified();
}

std::vector<PlatformTrustStore::CertWithTrust>
TrustStoreNSS::GetAllUserAddedCerts() {
  std::vector<PlatformTrustStore::CertWithTrust> user_added_certs;
  std::vector<ListCertsResult> certs = ListCertsIgnoringNSSRootsImpl();
  for (const auto& cert_result : certs) {
    // Skip user certs, unless the user added the user cert with specific
    // server auth trust settings.
    if (cert_result.trust.HasUnspecifiedTrust() &&
        CERT_IsUserCert(cert_result.cert.get())) {
      continue;
    }

    user_added_certs.emplace_back(
        base::ToVector(
            x509_util::CERTCertificateAsSpan(cert_result.cert.get())),
        cert_result.trust);
  }

  return user_added_certs;
}

}  // namespace net
