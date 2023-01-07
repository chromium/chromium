// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_AND_CT_VERIFIER_H_
#define NET_CERT_CERT_AND_CT_VERIFIER_H_

#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/log/net_log_with_source.h"

namespace net {

class CTVerifier;

// CertVerifier that also performs certificate transparency (CT) verification.
class NET_EXPORT CertAndCTVerifier : public CertVerifier {
 public:
  // Creates a CertAndCTVerifier that will use |cert_verifier| to perform the
  // actual underlying cert verification and |ct_verifier| to perform the CT
  // verification.
  CertAndCTVerifier(std::unique_ptr<CertVerifier> cert_verifier,
                    std::unique_ptr<CTVerifier> ct_verifier);

  ~CertAndCTVerifier() override;
  CertAndCTVerifier(const CertAndCTVerifier&) = delete;
  CertAndCTVerifier& operator=(const CertAndCTVerifier&) = delete;

  // CertVerifier implementation:
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;

 private:
  void OnCertVerifyComplete(const RequestParams& params,
                            CompletionOnceCallback callback,
                            CertVerifyResult* verify_result,
                            const NetLogWithSource& net_log,
                            int result);

  // TODO(crbug.com/1211074): Expose CT log list as part of
  // CertVerifier::Config.
  std::unique_ptr<CertVerifier> cert_verifier_;
  std::unique_ptr<CTVerifier> ct_verifier_;
};

}  // namespace net

#endif  // NET_CERT_CERT_AND_CT_VERIFIER_H_
