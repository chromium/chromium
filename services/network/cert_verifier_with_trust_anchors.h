// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_
#define SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertVerifyProc;
class CertVerifyResult;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace network {

// Wraps a MultiThreadedCertVerifier to make it use the additional trust anchors
// configured by the ONC user policy.
class COMPONENT_EXPORT(NETWORK_SERVICE) CertVerifierWithTrustAnchors
    : public net::CertVerifier {
 public:
  // Except of the constructor, all methods and the destructor must be called on
  // the IO thread. Calls |anchor_used_callback| on the IO thread every time a
  // certificate from the additional trust anchors (set with SetTrustAnchors) is
  // used.
  explicit CertVerifierWithTrustAnchors(
      const base::Closure& anchor_used_callback);
  ~CertVerifierWithTrustAnchors() override;

  // TODO(jam): once the network service is the only path, rename or get rid of
  // this method.
  void InitializeOnIOThread(
      const scoped_refptr<net::CertVerifyProc>& verify_proc);

  // Sets the additional trust anchors.
  void SetTrustAnchors(const net::CertificateList& trust_anchors);

  // CertVerifier:
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;

 private:
  net::CertVerifier::Config orig_config_;
  net::CertificateList trust_anchors_;
  base::Closure anchor_used_callback_;
  std::unique_ptr<CertVerifier> delegate_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CertVerifierWithTrustAnchors);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CERT_VERIFIER_WITH_TRUST_ANCHORS_H_
