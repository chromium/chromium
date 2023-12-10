// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_ANDROID_H_
#define NET_CERT_CERT_VERIFY_PROC_ANDROID_H_

#include "net/base/net_export.h"
#include "net/cert/cert_verify_proc.h"

namespace net {

class CertNetFetcher;

// Performs certificate verification on Android by calling the platform
// TrustManager through JNI.
class NET_EXPORT CertVerifyProcAndroid : public CertVerifyProc {
 public:
  explicit CertVerifyProcAndroid(scoped_refptr<CertNetFetcher> net_fetcher,
                                 scoped_refptr<CRLSet> crl_set);

  CertVerifyProcAndroid(const CertVerifyProcAndroid&) = delete;
  CertVerifyProcAndroid& operator=(const CertVerifyProcAndroid&) = delete;

 protected:
  ~CertVerifyProcAndroid() override;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  scoped_refptr<CertNetFetcher> cert_net_fetcher_;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_ANDROID_H_
