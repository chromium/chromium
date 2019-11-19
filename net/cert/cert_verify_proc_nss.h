// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_NSS_H_
#define NET_CERT_CERT_VERIFY_PROC_NSS_H_

#include <certt.h>

#include "net/base/net_export.h"
#include "net/cert/cert_verify_proc.h"

namespace net {

// Performs certificate path construction and validation using NSS's libpkix.
class NET_EXPORT_PRIVATE CertVerifyProcNSS : public CertVerifyProc {
 public:
  CertVerifyProcNSS();

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcNSS() override;

  // Like VerifyInternal, but adds a |chain_verify_callback| to override trust
  // decisions. See the documentation for CERTChainVerifyCallback and
  // CERTChainVerifyCallbackFunc in NSS's lib/certdb/certt.h.
  int VerifyInternalImpl(X509Certificate* cert,
                         const std::string& hostname,
                         const std::string& ocsp_response,
                         int flags,
                         CRLSet* crl_set,
                         const CertificateList& additional_trust_anchors,
                         CERTChainVerifyCallback* chain_verify_callback,
                         CertVerifyResult* verify_result);

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CRLSet* crl_set,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result) override;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_NSS_H_
