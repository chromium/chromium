// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_MAC_H_
#define NET_CERT_CERT_VERIFY_PROC_MAC_H_

#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verify_proc.h"

namespace net {

// Performs certificate path construction and validation using OS X's
// Security.framework.
class NET_EXPORT_PRIVATE CertVerifyProcMac : public CertVerifyProc {
 public:
  class ResultDebugData : public base::SupportsUserData::Data {
   public:
    struct CertEvidenceInfo {
      CertEvidenceInfo();
      ~CertEvidenceInfo();
      CertEvidenceInfo(const CertEvidenceInfo&);
      CertEvidenceInfo(CertEvidenceInfo&&);

      // A bitfield indicating various status of the cert, defined in
      // cssmapple.h
      uint32_t status_bits;
      // CSSM_RETURN status codes for the cert, defined in cssmtype.h, values in
      // cssmerr.h and cssmErrorStrings.h.
      std::vector<int32_t> status_codes;
    };

    ResultDebugData(uint32_t trust_result,
                    int32_t result_code,
                    std::vector<CertEvidenceInfo> status_chain);
    ~ResultDebugData() override;
    ResultDebugData(const ResultDebugData&);

    static const ResultDebugData* Get(const base::SupportsUserData* debug_data);
    static void Create(uint32_t trust_result,
                       int32_t result_code,
                       std::vector<CertEvidenceInfo> status_chain,
                       base::SupportsUserData* debug_data);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    uint32_t trust_result() const { return trust_result_; }
    int32_t result_code() const { return result_code_; }
    const std::vector<CertEvidenceInfo>& status_chain() const {
      return status_chain_;
    }

   private:
    // The SecTrustResultType result from SecTrustEvaluate.
    uint32_t trust_result_;
    // The OSStatus resultCode from SecTrustGetCssmResultCode.
    int32_t result_code_;
    // The CSSM_TP_APPLE_EVIDENCE_INFO statusChain from SecTrustGetResult. Each
    // entry corresponds to one of the certs in the verified chain (leaf first).
    std::vector<CertEvidenceInfo> status_chain_;
  };

  CertVerifyProcMac();

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcMac() override;

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

#endif  // NET_CERT_CERT_VERIFY_PROC_MAC_H_
