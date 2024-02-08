// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/mock_cert_verifier.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {
// Helper function for setting the appropriate CertStatus given a net::Error.
CertStatus MapNetErrorToCertStatus(int error) {
  switch (error) {
    case ERR_CERT_COMMON_NAME_INVALID:
      return CERT_STATUS_COMMON_NAME_INVALID;
    case ERR_CERT_DATE_INVALID:
      return CERT_STATUS_DATE_INVALID;
    case ERR_CERT_AUTHORITY_INVALID:
      return CERT_STATUS_AUTHORITY_INVALID;
    case ERR_CERT_NO_REVOCATION_MECHANISM:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;
    case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
    case ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      return CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
    case ERR_CERT_REVOKED:
      return CERT_STATUS_REVOKED;
    case ERR_CERT_INVALID:
      return CERT_STATUS_INVALID;
    case ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      return CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
    case ERR_CERT_NON_UNIQUE_NAME:
      return CERT_STATUS_NON_UNIQUE_NAME;
    case ERR_CERT_WEAK_KEY:
      return CERT_STATUS_WEAK_KEY;
    case ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return CERT_STATUS_PINNED_KEY_MISSING;
    case ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      return CERT_STATUS_NAME_CONSTRAINT_VIOLATION;
    case ERR_CERT_VALIDITY_TOO_LONG:
      return CERT_STATUS_VALIDITY_TOO_LONG;
    case ERR_CERT_SYMANTEC_LEGACY:
      return CERT_STATUS_SYMANTEC_LEGACY;
    case ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
      return (CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED | CERT_STATUS_REVOKED);
    default:
      return 0;
  }
}
}  // namespace

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
  MockRequest(MockCertVerifier* parent,
              CertVerifyResult* result,
              CompletionOnceCallback callback)
      : result_(result), callback_(std::move(callback)) {
    subscription_ = parent->request_list_.Add(
        base::BindOnce(&MockRequest::Cleanup, weak_factory_.GetWeakPtr()));
  }

  void ReturnResultLater(int rv, const CertVerifyResult& result) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockRequest::ReturnResult,
                                  weak_factory_.GetWeakPtr(), rv, result));
  }

 private:
  void ReturnResult(int rv, const CertVerifyResult& result) {
    // If the MockCertVerifier has been deleted, the callback will have been
    // reset to null.
    if (!callback_)
      return;

    *result_ = result;
    std::move(callback_).Run(rv);
  }

  void Cleanup() {
    // Note: May delete |this_|.
    std::move(callback_).Reset();
  }

  raw_ptr<CertVerifyResult> result_;
  CompletionOnceCallback callback_;
  base::CallbackListSubscription subscription_;

  base::WeakPtrFactory<MockRequest> weak_factory_{this};
};

MockCertVerifier::MockCertVerifier() = default;

MockCertVerifier::~MockCertVerifier() {
  // Reset the callbacks for any outstanding MockRequests to fulfill the
  // respective net::CertVerifier contract.
  request_list_.Notify();
}

int MockCertVerifier::Verify(const RequestParams& params,
                             CertVerifyResult* verify_result,
                             CompletionOnceCallback callback,
                             std::unique_ptr<Request>* out_req,
                             const NetLogWithSource& net_log) {
  if (!async_) {
    return VerifyImpl(params, verify_result);
  }

  auto request =
      std::make_unique<MockRequest>(this, verify_result, std::move(callback));
  CertVerifyResult result;
  int rv = VerifyImpl(params, &result);
  request->ReturnResultLater(rv, result);
  *out_req = std::move(request);
  return ERR_IO_PENDING;
}

void MockCertVerifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockCertVerifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

void MockCertVerifier::SimulateOnCertVerifierChanged() {
  for (Observer& observer : observers_) {
    observer.OnCertVerifierChanged();
  }
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

ParamRecordingMockCertVerifier::ParamRecordingMockCertVerifier() = default;
ParamRecordingMockCertVerifier::~ParamRecordingMockCertVerifier() = default;

int ParamRecordingMockCertVerifier::Verify(const RequestParams& params,
                                           CertVerifyResult* verify_result,
                                           CompletionOnceCallback callback,
                                           std::unique_ptr<Request>* out_req,
                                           const NetLogWithSource& net_log) {
  params_.push_back(params);
  return MockCertVerifier::Verify(params, verify_result, std::move(callback),
                                  out_req, net_log);
}

CertVerifierObserverCounter::CertVerifierObserverCounter(
    CertVerifier* verifier) {
  obs_.Observe(verifier);
}

CertVerifierObserverCounter::~CertVerifierObserverCounter() = default;

void CertVerifierObserverCounter::OnCertVerifierChanged() {
  change_count_++;
}

}  // namespace net
