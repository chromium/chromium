// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_H_
#define NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_H_

#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/internal/trust_store.h"

namespace net {

// A very simple implementation of a TrustStore, which contains a set of
// certificates and their trustedness.
class NET_EXPORT TrustStoreInMemory : public TrustStore {
 public:
  TrustStoreInMemory();
  ~TrustStoreInMemory() override;

  // Empties the trust store, resetting it to original state.
  void Clear();

  // Adds a certificate as a trust anchor (only the SPKI and subject will be
  // used during verification).
  void AddTrustAnchor(scoped_refptr<ParsedCertificate> cert);

  // Adds a certificate as a trust achor and extracts anchor constraints from
  // the certificate. See VerifyCertificateChain for details.
  void AddTrustAnchorWithConstraints(scoped_refptr<ParsedCertificate> cert);

  // TODO(eroman): This is marked "ForTest" as the current implementation
  // requires an exact match on the certificate DER (a wider match by say
  // issuer/serial is probably what we would want for a real implementation).
  void AddDistrustedCertificateForTest(scoped_refptr<ParsedCertificate> cert);

  // Adds a certificate to the store, that is neither trusted nor untrusted.
  void AddCertificateWithUnspecifiedTrust(
      scoped_refptr<ParsedCertificate> cert);

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  void GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                CertificateTrust* trust,
                base::SupportsUserData* debug_data) const override;

  // Returns true if the trust store contains the given ParsedCertificate
  // (matches by DER).
  bool Contains(const ParsedCertificate* cert) const;

 private:
  struct Entry {
    Entry();
    Entry(const Entry& other);
    ~Entry();

    scoped_refptr<ParsedCertificate> cert;
    CertificateTrust trust;
  };

  // Multimap from normalized subject -> Entry.
  std::unordered_multimap<base::StringPiece, Entry, base::StringPieceHash>
      entries_;

  // Adds a certificate with the specified trust settings. Both trusted and
  // distrusted certificates require a full DER match.
  void AddCertificate(scoped_refptr<ParsedCertificate> cert,
                      const CertificateTrust& trust);

  DISALLOW_COPY_AND_ASSIGN(TrustStoreInMemory);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_H_
