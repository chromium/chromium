// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_JOB_H_
#define NET_QUIC_QUIC_SESSION_POOL_JOB_H_

#include "base/memory/raw_ptr.h"
#include "net/base/net_error_details.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"

namespace net {

// Responsible for creating a new QUIC session to the specified server, and for
// notifying any associated requests when complete.
//
// A single Job can be associated with any number of `QuicSessionRequest`
// instances, and will update all of them with its progress, calling some or
// all of their `OnHostResolution()` or `OnQuicSessionCreationComplete()`
// methods, as indicated by calls to `ExpectOnHostResolution()` and
// `ExpectQuicSessionCreation()`, respectively.
//
// When the session is confirmed, the job will call the pool's
// `ActivateSession` method before invoking the callback from `Run`.
//
// The |client_config_handle| is not actually used, but serves to keep the
// corresponding CryptoClientConfig alive until the Job completes.
class QuicSessionPool::Job : public QuicSessionAttempt::Delegate {
 public:
  Job(QuicSessionPool* pool,
      QuicSessionAliasKey key,
      std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
      RequestPriority priority,
      const NetLogWithSource& net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() override;

  // Run the job. This should be called as soon as the job is created, then any
  // associated requests added with `AddRequest()`.
  virtual int Run(CompletionOnceCallback callback) = 0;

  // Add a new request associated with this Job.
  void AddRequest(QuicSessionRequest* request);

  // Remove a request associated with this job. The request must already be
  // associated with the job.
  void RemoveRequest(QuicSessionRequest* request);

  // Update the priority of this Job.
  void SetPriority(RequestPriority priority);

  // Add information about the new session to `details`. Must be called after
  // the run has completed.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) const = 0;

  const QuicSessionAliasKey& key() const { return key_; }
  const NetLogWithSource& net_log() const { return net_log_; }
  const std::set<raw_ptr<QuicSessionRequest, SetExperimental>>& requests() {
    return requests_;
  }
  RequestPriority priority() const { return priority_; }
  QuicSessionPool* pool() const { return pool_.get(); }

  // Associate this job with another source.
  void AssociateWithNetLogSource(
      const NetLogWithSource& http_stream_job_net_log) const;

  // QuicSessionAttempt::Delegate implementation.
  QuicSessionPool* GetQuicSessionPool() override;
  const QuicSessionAliasKey& GetKey() override;
  const NetLogWithSource& GetNetLog() override;
  void OnConnectionFailedOnDefaultNetwork() override;
  void OnQuicSessionCreationComplete(int rv) override;

 protected:
  // Set a new `QuicSessionRequest`'s expectations about which callbacks
  // will be invoked. This is called in `AddRequest`.
  virtual void SetRequestExpectations(QuicSessionRequest* request) = 0;

  // Update the priority of any ongoing work in this job.
  virtual void UpdatePriority(RequestPriority old_priority,
                              RequestPriority new_priority);

  const raw_ptr<QuicSessionPool> pool_;
  const QuicSessionAliasKey key_;
  const std::unique_ptr<CryptoClientConfigHandle> client_config_handle_;
  RequestPriority priority_;
  const NetLogWithSource net_log_;
  std::set<raw_ptr<QuicSessionRequest, SetExperimental>> requests_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_JOB_H_
