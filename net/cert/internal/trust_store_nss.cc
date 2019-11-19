// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>

#include "crypto/nss_util.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"

namespace net {

TrustStoreNSS::TrustStoreNSS(SECTrustType trust_type)
    : trust_type_(trust_type), filter_trusted_certs_by_slot_(false) {}

TrustStoreNSS::TrustStoreNSS(SECTrustType trust_type,
                             crypto::ScopedPK11Slot user_slot)
    : trust_type_(trust_type),
      filter_trusted_certs_by_slot_(true),
      user_slot_(std::move(user_slot)) {
  DCHECK(user_slot_);
}

TrustStoreNSS::TrustStoreNSS(
    SECTrustType trust_type,
    DisallowTrustForCertsOnUserSlots disallow_trust_for_certs_on_user_slots)
    : trust_type_(trust_type), filter_trusted_certs_by_slot_(true) {}

TrustStoreNSS::~TrustStoreNSS() = default;

void TrustStoreNSS::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
  crypto::EnsureNSSInit();

  SECItem name;
  // Use the original issuer value instead of the normalized version. NSS does a
  // less extensive normalization in its Name comparisons, so our normalized
  // version may not match the unnormalized version.
  name.len = cert->tbs().issuer_tlv.Length();
  name.data = const_cast<uint8_t*>(cert->tbs().issuer_tlv.UnsafeData());
  // |validOnly| in CERT_CreateSubjectCertList controls whether to return only
  // certs that are valid at |sorttime|. Expiration isn't meaningful for trust
  // anchors, so request all the matches.
  CERTCertList* found_certs = CERT_CreateSubjectCertList(
      nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
      PR_Now() /* sorttime */, PR_FALSE /* validOnly */);
  if (!found_certs)
    return;

  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
#if !defined(OS_CHROMEOS)
    // TODO(mattm): use CERT_GetCertIsTemp when minimum NSS version is >= 3.31.
    if (node->cert->istemp) {
      // Ignore temporary NSS certs on platforms other than Chrome OS. This
      // ensures that during the trial when CertVerifyProcNSS and
      // CertVerifyProcBuiltin are being used simultaneously, the builtin
      // verifier does not get to "cheat" by using AIA fetched certs from
      // CertVerifyProcNSS.
      // TODO(https://crbug.com/951479): remove this when CertVerifyProcBuiltin
      // becomes the default.
      // This is not done for Chrome OS because temporary NSS certs are
      // currently used there to implement policy-provided untrusted authority
      // certificates, and no trials are being done on Chrome OS.
      // TODO(https://crbug.com/978854): remove the Chrome OS exception when
      // certificates are passed.
      continue;
    }
#endif  // !defined(OS_CHROMEOS)

    CertErrors parse_errors;
    scoped_refptr<ParsedCertificate> cur_cert = ParsedCertificate::Create(
        x509_util::CreateCryptoBuffer(node->cert->derCert.data,
                                      node->cert->derCert.len),
        {}, &parse_errors);

    if (!cur_cert) {
      // TODO(crbug.com/634443): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << parse_errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(cur_cert));
  }
  CERT_DestroyCertList(found_certs);
}

void TrustStoreNSS::GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                             CertificateTrust* out_trust,
                             base::SupportsUserData* debug_data) const {
  crypto::EnsureNSSInit();

  // TODO(eroman): Inefficient -- path building will convert between
  // CERTCertificate and ParsedCertificate representations multiple times
  // (when getting the issuers, and again here).

  // Note that trust records in NSS are keyed on issuer + serial, and there
  // exist builtin distrust records for which a matching certificate is not
  // included in the builtin cert list. Therefore, create a temp NSS cert even
  // if no existing cert matches. (Eg, this uses CERT_NewTempCertificate, not
  // CERT_FindCertByDERCert.)
  ScopedCERTCertificate nss_cert(x509_util::CreateCERTCertificateFromBytes(
      cert->der_cert().UnsafeData(), cert->der_cert().Length()));
  if (!nss_cert) {
    *out_trust = CertificateTrust::ForUnspecified();
    return;
  }

  if (!IsCertAllowedForTrust(nss_cert.get())) {
    *out_trust = CertificateTrust::ForUnspecified();
    return;
  }

  // Determine the trustedness of the matched certificate.
  CERTCertTrust trust;
  if (CERT_GetCertTrust(nss_cert.get(), &trust) != SECSuccess) {
    *out_trust = CertificateTrust::ForUnspecified();
    return;
  }

  int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trust_type_);

  // Determine if the certificate is distrusted.
  if ((trust_flags & (CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED_CA |
                      CERTDB_TRUSTED)) == CERTDB_TERMINAL_RECORD) {
    *out_trust = CertificateTrust::ForDistrusted();
    return;
  }

  // Determine if the certificate is a trust anchor.
  if ((trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
    *out_trust = CertificateTrust::ForTrustAnchor();
    return;
  }

  // Trusted server certs (CERTDB_TERMINAL_RECORD + CERTDB_TRUSTED) are
  // intentionally treated as unspecified. See https://crbug.com/814994.

  *out_trust = CertificateTrust::ForUnspecified();
  return;
}

bool TrustStoreNSS::IsCertAllowedForTrust(CERTCertificate* cert) const {
  // If |filter_trusted_certs_by_slot_| is false, allow trust for any
  // certificate, no matter which slot it is stored on.
  if (!filter_trusted_certs_by_slot_)
    return true;

  crypto::ScopedPK11SlotList slots_for_cert(
      PK11_GetAllSlotsForCert(cert, nullptr));
  if (!slots_for_cert)
    return false;

  for (PK11SlotListElement* slot_element =
           PK11_GetFirstSafe(slots_for_cert.get());
       slot_element;
       slot_element = PK11_GetNextSafe(slots_for_cert.get(), slot_element,
                                       /*restart=*/PR_FALSE)) {
    PK11SlotInfo* slot = slot_element->slot;
    bool allow_slot =
        // Allow the root certs module.
        PK11_HasRootCerts(slot) ||
        // Allow read-only internal slots.
        (PK11_IsInternal(slot) && !PK11_IsRemovable(slot)) ||
        // Allow |user_slot_| if specified.
        (user_slot_ && slot == user_slot_.get());

    if (allow_slot) {
      PK11_FreeSlotListElement(slots_for_cert.get(), slot_element);
      return true;
    }
  }

  return false;
}

}  // namespace net
