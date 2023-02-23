// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/known_roots_nss.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"

namespace net {

TrustStoreNSS::TrustStoreNSS(SECTrustType trust_type,
                             SystemTrustSetting system_trust_setting,
                             UserSlotTrustSetting user_slot_trust_setting)
    : trust_type_(trust_type),
      ignore_system_trust_settings_(system_trust_setting == kIgnoreSystemTrust),
      user_slot_trust_setting_(std::move(user_slot_trust_setting)) {}

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
  crypto::ScopedCERTCertList found_certs(CERT_CreateSubjectCertList(
      nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
      PR_Now() /* sorttime */, PR_FALSE /* validOnly */));
  if (!found_certs)
    return;

  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
    CertErrors parse_errors;
    std::shared_ptr<const ParsedCertificate> cur_cert =
        ParsedCertificate::Create(
            x509_util::CreateCryptoBuffer(base::make_span(
                node->cert->derCert.data, node->cert->derCert.len)),
            {}, &parse_errors);

    if (!cur_cert) {
      // TODO(crbug.com/634443): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << parse_errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(cur_cert));
  }
}

CertificateTrust TrustStoreNSS::GetTrust(const ParsedCertificate* cert,
                                         base::SupportsUserData* debug_data) {
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
    return CertificateTrust::ForUnspecified();
  }

  if (!IsCertAllowedForTrust(nss_cert.get())) {
    return CertificateTrust::ForUnspecified();
  }

  // Determine the trustedness of the matched certificate.
  CERTCertTrust trust;
  if (CERT_GetCertTrust(nss_cert.get(), &trust) != SECSuccess) {
    return CertificateTrust::ForUnspecified();
  }

  unsigned int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trust_type_);

  // Determine if the certificate is distrusted.
  if ((trust_flags & (CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED_CA |
                      CERTDB_TRUSTED)) == CERTDB_TERMINAL_RECORD) {
    return CertificateTrust::ForDistrusted();
  }

  bool is_trusted_ca = false;
  bool is_trusted_leaf = false;
  bool enforce_anchor_constraints =
      IsLocalAnchorConstraintsEnforcementEnabled();

  // Determine if the certificate is a trust anchor.
  //
  // We may not use the result of this if it is a known root and we're ignoring
  // system certs.
  if ((trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
    // If its a user root, or its a system root and we're not ignoring system
    // roots than return root as trusted.
    //
    // TODO(hchao, sleevi): CERT_GetCertTrust combines the trust settings from
    // all tokens and slots, meaning it doesn't allow us to distinguish between
    // CKO_NSS_TRUST objects the user manually configured versus CKO_NSS_TRUST
    // objects from the builtin token (system trust settings). Properly
    // handling this may require iterating all the slots and manually computing
    // the trust settings directly, rather than CERT_GetCertTrust.
    if (ignore_system_trust_settings_) {
      // Only trust the user roots, and apply the value of
      // enforce_anchor_constraints.
      if (!IsKnownRoot(nss_cert.get())) {
        is_trusted_ca = true;
      }
    } else {
      is_trusted_ca = true;
      if (enforce_anchor_constraints && IsKnownRoot(nss_cert.get())) {
        // Don't enforce anchor constraints on the builtin roots. Needing to
        // check IsKnownRoot for this condition isn't ideal, but this should be
        // good enough for now.
        enforce_anchor_constraints = false;
      }
    }
  }

  if (base::FeatureList::IsEnabled(features::kTrustStoreTrustedLeafSupport)) {
    constexpr unsigned int kTrustedPeerBits =
        CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
    if ((trust_flags & kTrustedPeerBits) == kTrustedPeerBits) {
      is_trusted_leaf = true;
    }
  }

  if (is_trusted_ca && is_trusted_leaf) {
    return CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorConstraints(enforce_anchor_constraints)
        .WithEnforceAnchorExpiry(enforce_anchor_constraints);
  } else if (is_trusted_ca) {
    return CertificateTrust::ForTrustAnchor()
        .WithEnforceAnchorConstraints(enforce_anchor_constraints)
        .WithEnforceAnchorExpiry(enforce_anchor_constraints);
  } else if (is_trusted_leaf) {
    return CertificateTrust::ForTrustedLeaf();
  }

  return CertificateTrust::ForUnspecified();
}

bool TrustStoreNSS::IsCertAllowedForTrust(CERTCertificate* cert) const {
  if (absl::holds_alternative<UseTrustFromAllUserSlots>(
          user_slot_trust_setting_)) {
    return true;
  }

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
        // Allow configured user slot if specified.
        (absl::holds_alternative<crypto::ScopedPK11Slot>(
             user_slot_trust_setting_) &&
         slot ==
             absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_).get());

    if (allow_slot) {
      PK11_FreeSlotListElement(slots_for_cert.get(), slot_element);
      return true;
    }
  }

  return false;
}

}  // namespace net
