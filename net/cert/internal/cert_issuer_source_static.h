// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_STATIC_H_
#define NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_STATIC_H_

#include <unordered_map>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/internal/cert_issuer_source.h"

namespace net {

// Synchronously returns issuers from a pre-supplied set.
class NET_EXPORT CertIssuerSourceStatic : public CertIssuerSource {
 public:
  CertIssuerSourceStatic();

  CertIssuerSourceStatic(const CertIssuerSourceStatic&) = delete;
  CertIssuerSourceStatic& operator=(const CertIssuerSourceStatic&) = delete;

  ~CertIssuerSourceStatic() override;

  // Adds |cert| to the set of certificates that this CertIssuerSource will
  // provide.
  void AddCert(scoped_refptr<ParsedCertificate> cert);

  // CertIssuerSource implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  void AsyncGetIssuersOf(const ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) override;

 private:
  // The certificates that the CertIssuerSourceStatic can return, keyed on the
  // normalized subject value.
  std::unordered_multimap<base::StringPiece,
                          scoped_refptr<ParsedCertificate>,
                          base::StringPieceHash>
      intermediates_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_STATIC_H_
