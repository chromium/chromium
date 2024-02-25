// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_
#define SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertVerifyResult;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace network {

// Wraps a net::CertVerifier to run a callback if the additional trust anchors
// configured by the ONC user policy are used.
class COMPONENT_EXPORT(NETWORK_SERVICE) CertVerifierWithTrustAnchors
    : public net::CertVerifier {
 public:
  // Except of the constructor, all methods and the destructor must be called on
  // the IO thread. Calls |anchor_used_callback| on the IO thread every time a
  // certificate from the additional trust anchors (set with SetTrustAnchors) is
  // used.
  explicit CertVerifierWithTrustAnchors(
      const base::RepeatingClosure& anchor_used_callback);

  CertVerifierWithTrustAnchors(const CertVerifierWithTrustAnchors&) = delete;
  CertVerifierWithTrustAnchors& operator=(const CertVerifierWithTrustAnchors&) =
      delete;

  ~CertVerifierWithTrustAnchors() override;

  // TODO(jam): once the network service is the only path, rename or get rid of
  // this method.
  void InitializeOnIOThread(std::unique_ptr<net::CertVerifier> delegate);

  // CertVerifier:
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  base::RepeatingClosure anchor_used_callback_;
  std::unique_ptr<CertVerifier> delegate_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_
