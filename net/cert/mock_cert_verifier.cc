// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/mock_cert_verifier.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"

namespace net {

struct MockCertVerifier::Rule {
  Rule(scoped_refptr<X509Certificate> cert_arg,
       const std::string& hostname_arg,
       const CertVerifyResult& result_arg,
       int rv_arg)
      : cert(std::move(cert_arg)),
        hostname(hostname_arg),
        result(result_arg),
        rv(rv_arg) {
    DCHECK(cert);
    DCHECK(result.verified_cert);
  }

  scoped_refptr<X509Certificate> cert;
  std::string hostname;
  CertVerifyResult result;
  int rv;
};

class MockCertVerifier::MockRequest : public CertVerifier::Request {
 public:
  MockRequest(CertVerifyResult* result, CompletionOnceCallback callback)
      : result_(result), callback_(std::move(callback)) {}

  void ReturnResultLater(int rv, const CertVerifyResult& result) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MockRequest::ReturnResult,
                                  weak_factory_.GetWeakPtr(), rv, result));
  }

 private:
  void ReturnResult(int rv, const CertVerifyResult& result) {
    *result_ = result;
    std::move(callback_).Run(rv);
  }

  CertVerifyResult* result_;
  CompletionOnceCallback callback_;
  base::WeakPtrFactory<MockRequest> weak_factory_{this};
};

MockCertVerifier::MockCertVerifier()
    : default_result_(ERR_CERT_INVALID), async_(false) {}

MockCertVerifier::~MockCertVerifier() = default;

int MockCertVerifier::Verify(const RequestParams& params,
                             CertVerifyResult* verify_result,
                             CompletionOnceCallback callback,
                             std::unique_ptr<Request>* out_req,
                             const NetLogWithSource& net_log) {
  if (!async_) {
    return VerifyImpl(params, verify_result);
  }

  auto request =
      std::make_unique<MockRequest>(verify_result, std::move(callback));
  CertVerifyResult result;
  int rv = VerifyImpl(params, &result);
  request->ReturnResultLater(rv, result);
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void MockCertVerifier::AddResultForCert(scoped_refptr<X509Certificate> cert,
                                        const CertVerifyResult& verify_result,
                                        int rv) {
  AddResultForCertAndHost(std::move(cert), "*", verify_result, rv);
}

void MockCertVerifier::AddResultForCertAndHost(
    scoped_refptr<X509Certificate> cert,
    const std::string& host_pattern,
    const CertVerifyResult& verify_result,
    int rv) {
  rules_.push_back(Rule(std::move(cert), host_pattern, verify_result, rv));
}

void MockCertVerifier::ClearRules() {
  rules_.clear();
}

int MockCertVerifier::VerifyImpl(const RequestParams& params,
                                 CertVerifyResult* verify_result) {
  for (const Rule& rule : rules_) {
    // Check just the server cert. Intermediates will be ignored.
    if (!rule.cert->EqualsExcludingChain(params.certificate().get()))
      continue;
    if (!base::MatchPattern(params.hostname(), rule.hostname))
      continue;
    *verify_result = rule.result;
    return rule.rv;
  }

  // Fall through to the default.
  verify_result->verified_cert = params.certificate();
  verify_result->cert_status = MapNetErrorToCertStatus(default_result_);
  return default_result_;
}

}  // namespace net
