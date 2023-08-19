// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_
#define NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/cert/pki/cert_issuer_source.h"

namespace net {

class CertNetFetcher;

class NET_EXPORT CertIssuerSourceAia : public CertIssuerSource {
 public:
  class NET_EXPORT AiaDebugData : public base::SupportsUserData::Data {
   public:
    static const AiaDebugData* Get(const base::SupportsUserData* debug_data);
    static AiaDebugData* GetOrCreate(base::SupportsUserData* debug_data);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    void IncrementAiaFetchSuccess() { aia_fetch_success_++; }
    void IncrementAiaFetchFail() { aia_fetch_fail_++; }

    int aia_fetch_fail() const { return aia_fetch_fail_; }
    int aia_fetch_success() const { return aia_fetch_success_; }

   private:
    int aia_fetch_success_ = 0;
    int aia_fetch_fail_ = 0;
  };

  // Creates CertIssuerSource that will use |cert_fetcher| to retrieve issuers
  // using AuthorityInfoAccess URIs. CertIssuerSourceAia must be created and
  // used only on a single thread, which is the thread |cert_fetcher| will be
  // operated from.
  explicit CertIssuerSourceAia(scoped_refptr<CertNetFetcher> cert_fetcher);

  CertIssuerSourceAia(const CertIssuerSourceAia&) = delete;
  CertIssuerSourceAia& operator=(const CertIssuerSourceAia&) = delete;

  ~CertIssuerSourceAia() override;

  // CertIssuerSource implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  void AsyncGetIssuersOf(const ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) override;

 private:
  scoped_refptr<CertNetFetcher> cert_fetcher_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_CERT_ISSUER_SOURCE_AIA_H_
