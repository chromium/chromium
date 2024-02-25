// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_
#define NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/pki/cert_issuer_source.h"

namespace net {

class CertNetFetcher;

class NET_EXPORT CertIssuerSourceAia : public bssl::CertIssuerSource {
 public:
  // Creates bssl::CertIssuerSource that will use |cert_fetcher| to retrieve
  // issuers using AuthorityInfoAccess URIs. CertIssuerSourceAia must be created
  // and used only on a single thread, which is the thread |cert_fetcher| will
  // be operated from.
  explicit CertIssuerSourceAia(scoped_refptr<CertNetFetcher> cert_fetcher);

  CertIssuerSourceAia(const CertIssuerSourceAia&) = delete;
  CertIssuerSourceAia& operator=(const CertIssuerSourceAia&) = delete;

  ~CertIssuerSourceAia() override;

  // bssl::CertIssuerSource implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  void AsyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) override;

 private:
  scoped_refptr<CertNetFetcher> cert_fetcher_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_
