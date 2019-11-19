// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
#define NET_CERT_INTERNAL_TRUST_STORE_NSS_H_

#include <cert.h>
#include <certt.h>

#include "base/memory/ref_counted.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/trust_store.h"

namespace net {

// TrustStoreNSS is an implementation of TrustStore which uses NSS to find trust
// anchors for path building.
class NET_EXPORT TrustStoreNSS : public TrustStore {
 public:
  struct DisallowTrustForCertsOnUserSlots {};

  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // |trust_type|.
  // The created TrustStoreNSS will not perform any filtering based on the slot
  // certificates are stored on.
  explicit TrustStoreNSS(SECTrustType trust_type);

  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // |trust_type|.
  // The created TrustStoreNSS will allow trust for certificates that:
  // (*) are built-in certificates
  // (*) are stored on a read-only internal slot
  // (*) are stored on the |user_slot|.
  TrustStoreNSS(SECTrustType trust_type, crypto::ScopedPK11Slot user_slot);

  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // |trust_type|.
  // The created TrustStoreNSS will allow trust for certificates that:
  // (*) are built-in certificates
  // (*) are stored on a read-only internal slot
  TrustStoreNSS(
      SECTrustType trust_type,
      DisallowTrustForCertsOnUserSlots disallow_trust_for_certs_on_user_slots);

  ~TrustStoreNSS() override;

  // CertIssuerSource implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  // TrustStore implementation:
  void GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                CertificateTrust* trust,
                base::SupportsUserData* debug_data) const override;

 private:
  bool IsCertAllowedForTrust(CERTCertificate* cert) const;

  SECTrustType trust_type_;

  // |filter_trusted_certs_by_slot_| and |user_slot_| together specify which
  // slots certificates must be stored on to be allowed to be trusted. The
  // possible combinations are:
  //
  // |filter_trusted_certs_by_slot_| == false: Allow any certificate to be
  // trusted, don't filter by slot. |user_slot_| is ignored in this case.
  //
  // |filter_trusted_certs_by_slot_| == true and |user_slot_| = nullptr: Allow
  // certificates to be trusted if they
  // (*) are built-in certificates or
  // (*) are stored on a read-only internal slot.
  //
  // |filter_trusted_certs_by_slot_| == true and |user_slot_| != nullptr: Allow
  // certificates to be trusted if they
  // (*) are built-in certificates or
  // (*) are stored on a read-only internal slot or
  // (*) are stored on |user_slot_|.
  const bool filter_trusted_certs_by_slot_;
  crypto::ScopedPK11Slot user_slot_;

  DISALLOW_COPY_AND_ASSIGN(TrustStoreNSS);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
