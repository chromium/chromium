// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_WIN_H_
#define NET_CERT_CERT_VERIFY_PROC_WIN_H_

#include "net/cert/cert_verify_proc.h"

namespace net {

// Performs certificate path construction and validation using Windows'
// CryptoAPI.
class NET_EXPORT_PRIVATE CertVerifyProcWin : public CertVerifyProc {
 public:
  CertVerifyProcWin();

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcWin() override;

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

#endif  // NET_CERT_CERT_VERIFY_PROC_WIN_H_
