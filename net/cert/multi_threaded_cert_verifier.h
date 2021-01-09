// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_MULTI_THREADED_CERT_VERIFIER_H_
#define NET_CERT_MULTI_THREADED_CERT_VERIFIER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"

#if defined(USE_NSS_CERTS)
#include "net/cert/scoped_nss_types.h"
#endif

namespace net {

class CertVerifyProc;

// MultiThreadedCertVerifier is a CertVerifier implementation that runs
// synchronous CertVerifier implementations on worker threads.
class NET_EXPORT_PRIVATE MultiThreadedCertVerifier : public CertVerifier {
 public:
  explicit MultiThreadedCertVerifier(scoped_refptr<CertVerifyProc> verify_proc);

  // When the verifier is destroyed, all certificate verifications requests are
  // canceled, and their completion callbacks will not be called.
  ~MultiThreadedCertVerifier() override;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const CertVerifier::Config& config) override;

 private:
  class InternalRequest;

  Config config_;
  scoped_refptr<CertVerifyProc> verify_proc_;

  // Holds a list of CertVerifier::Requests that have not yet completed or been
  // deleted. It is used to ensure that when the MultiThreadedCertVerifier is
  // deleted, we eagerly reset all of the callbacks provided to Verify(), and
  // don't call them later, as required by the CertVerifier contract.
  base::LinkedList<InternalRequest> request_list_;

#if defined(USE_NSS_CERTS)
  // Holds NSS temporary certificates that will be exposed as untrusted
  // authorities by SystemCertStoreNSS.
  // TODO(https://crbug.com/978854): Pass these into the actual CertVerifyProc
  // rather than relying on global side-effects.
  net::ScopedCERTCertificateList temp_certs_;
#endif

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MultiThreadedCertVerifier);
};

}  // namespace net

#endif  // NET_CERT_MULTI_THREADED_CERT_VERIFIER_H_
