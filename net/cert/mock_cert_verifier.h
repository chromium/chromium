// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MOCK_CERT_VERIFIER_H_
#define NET_CERT_MOCK_CERT_VERIFIER_H_

#include <list>
#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"

namespace net {

class MockCertVerifier : public CertVerifier {
 public:
  // Creates a new MockCertVerifier. By default, any call to Verify() will
  // result in the cert status being flagged as CERT_STATUS_INVALID and return
  // an ERR_CERT_INVALID network error code. This behaviour can be overridden
  // by calling set_default_result() to change the default return value for
  // Verify() or by calling one of the AddResult*() methods to specifically
  // handle a certificate or certificate and host.
  MockCertVerifier();

  ~MockCertVerifier() override;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override {}

  // Sets the default return value for Verify() for certificates/hosts that do
  // not have explicit results added via the AddResult*() methods.
  void set_default_result(int default_result) {
    default_result_ = default_result;
  }

  // Sets whether Verify() returns a result asynchronously.
  void set_async(bool async) { async_ = async; }

  // Adds a rule that will cause any call to Verify() for |cert| to return rv,
  // copying |verify_result| into the verified result.
  // Note: Only the primary certificate of |cert| is checked. Any intermediate
  // certificates will be ignored.
  void AddResultForCert(scoped_refptr<X509Certificate> cert,
                        const CertVerifyResult& verify_result,
                        int rv);

  // Same as AddResultForCert(), but further restricts it to only return for
  // hostnames that match |host_pattern|.
  void AddResultForCertAndHost(scoped_refptr<X509Certificate> cert,
                               const std::string& host_pattern,
                               const CertVerifyResult& verify_result,
                               int rv);

  // Clear all existing rules.
  void ClearRules();

 private:
  struct Rule;
  using RuleList = std::list<Rule>;
  class MockRequest;

  int VerifyImpl(const RequestParams& params, CertVerifyResult* verify_result);

  int default_result_;
  RuleList rules_;
  bool async_;
};

}  // namespace net

#endif  // NET_CERT_MOCK_CERT_VERIFIER_H_
