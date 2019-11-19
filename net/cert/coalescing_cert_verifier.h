// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_COALESCING_CERT_VERIFIER_H_
#define NET_CERT_COALESCING_CERT_VERIFIER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"

namespace net {

// CoalescingCertVerifier is a CertVerifier that keeps track of in-flight
// CertVerifier Verify() requests. If a new call to Verify() is started that
// matches the same parameters as an in-progress verification, the new
// Verify() call will be joined to the existing, in-progress verification,
// completing when it does. If no in-flight requests match, a new request to
// the underlying verifier will be started.
//
// If the underlying configuration changes, existing requests are allowed to
// complete, but any new requests will not be seen as matching, even if they
// share the same parameters. This ensures configuration changes propagate
// "immediately" for all new requests.
class NET_EXPORT CoalescingCertVerifier : public CertVerifier {
 public:
  // Create a new verifier that will forward calls to |verifier|, coalescing
  // any in-flight, not-yet-completed calls to Verify().
  explicit CoalescingCertVerifier(std::unique_ptr<CertVerifier> verifier);

  ~CoalescingCertVerifier() override;

  // CertVerifier implementation:
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<CertVerifier::Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const CertVerifier::Config& config) override;

  uint64_t requests_for_testing() const { return requests_; }
  uint64_t inflight_joins_for_testing() const { return inflight_joins_; }

 private:
  class Job;
  class Request;

  // If there is a pending request that matches |params|, and which can be
  // joined (it shares the same config), returns that Job.
  // Otherwise, returns nullptr, meaning a new Job should be started.
  Job* FindJob(const RequestParams& params);
  void RemoveJob(Job* job);

  // Contains the set of Jobs for which an active verification is taking
  // place and which can be used for new requests (e.g. the config is the
  // same).
  std::map<CertVerifier::RequestParams, std::unique_ptr<Job>> joinable_jobs_;

  // Contains all pending Jobs that are in-flight, but cannot be joined, due
  // to the configuration having changed since they were started.
  std::vector<std::unique_ptr<Job>> inflight_jobs_;

  std::unique_ptr<CertVerifier> verifier_;

  uint32_t config_id_;
  uint64_t requests_;
  uint64_t inflight_joins_;

  DISALLOW_COPY_AND_ASSIGN(CoalescingCertVerifier);
};

}  // namespace net

#endif  // NET_CERT_COALESCING_CERT_VERIFIER_H_
