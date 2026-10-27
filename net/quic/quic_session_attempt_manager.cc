// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_attempt_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/reconnect_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_endpoint.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_attempt_request.h"
#include "net/quic/quic_session_pool.h"

namespace net {

// A Job is responsible for creating a QUIC session for a specific
// QuicSessionAliasKey. It manages multiple concurrent connection attempts
// (`QuicSessionAttempt`) to different endpoints and notifies multiple clients
// (`QuicSessionAttemptRequest`) upon completion.
//
// If any attempt succeeds, the Job immediately notifies all waiting requests
// and cancels any other ongoing attempts. If an attempt fails, the Job will
// wait for other attempts to complete. Only when the last attempt fails does
// the Job notify all waiting requests of the failure.
//
// The Job is owned by the QuicSessionAttemptManager and is destroyed once the
// session is created or all attempts have failed.
class QuicSessionAttemptManager::Job : public QuicSessionAttempt::Delegate {
 public:
  Job(QuicSessionAttemptManager* manager,
      QuicSessionAliasKey key,
      const NetLogWithSource& net_log)
      : manager_(manager), key_(std::move(key)), net_log_(net_log) {}

  ~Job() override { Abort(); }

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  void Abort() {
    // Notify all pending requests that the job is aborted.
    if (!requests_.empty()) {
      NotifyRequests(ERR_ABORTED, /*session=*/nullptr, NetErrorDetails());
    }
    CancelAttempts();
  }

  // Attempts to create a QUIC session for the given endpoint. If an attempt
  // already exists for the endpoint, returns ERR_IO_PENDING and the request
  // will be notified when the attempt completes. Otherwise, a new attempt is
  // created and started, and the request will be notified when the attempt
  // completes.
  //
  // The request will be added to the job and notified upon completion.
  int MaybeAttemptEndpoint(
      QuicSessionAttemptRequest* request,
      QuicEndpoint endpoint,
      int cert_verify_flags,
      base::TimeTicks dns_resolution_start_time,
      base::TimeTicks dns_resolution_end_time,
      std::optional<ResolutionDetails> dns_resolution_details,
      bool use_dns_aliases,
      std::set<std::string> dns_aliases,
      MultiplexedSessionCreationInitiator session_creation_initiator,
      std::optional<ConnectionManagementConfig> connection_management_config) {
    AddRequest(request);

    if (FindAttempt(endpoint)) {
      return ERR_IO_PENDING;
    }

    std::unique_ptr<QuicSessionAttempt> attempt =
        manager_->pool_->CreateSessionAttempt(
            this, key_.session_key(), endpoint, cert_verify_flags,
            dns_resolution_start_time, dns_resolution_end_time,
            std::move(dns_resolution_details), use_dns_aliases,
            std::move(dns_aliases), session_creation_initiator,
            std::move(connection_management_config));
    QuicSessionAttempt* raw_attempt = attempt.get();
    auto [it, inserted] = attempts_.emplace(std::move(attempt));
    CHECK(inserted);
    int rv = raw_attempt->Start(base::BindOnce(
        &Job::OnAttemptComplete, base::Unretained(this), raw_attempt));
    if (rv != ERR_IO_PENDING) {
      // If the attempt failed synchronously but there are other attempts, wait
      // for them to complete.
      if (rv != OK && attempts_.size() > 1) {
        attempts_.erase(it);
        return ERR_IO_PENDING;
      }
      OnAttemptComplete(raw_attempt, rv);
    }
    return rv;
  }

  // Called by QuicSessionAttemptRequest to remove itself from the job.
  void RemoveRequest(QuicSessionAttemptRequest* request) {
    auto it = requests_.find(request);
    CHECK(it != requests_.end());
    requests_.erase(it);

    if (requests_.empty() && !notifying_requests_) {
      manager_->OnJobComplete(this);
      // `this` is deleted.
    }
  }

  bool HasRequest(QuicSessionAttemptRequest* request) const {
    return requests_.contains(request);
  }

  void OnOriginFrameMatched(QuicChromiumClientSession* session) {
    NotifyRequestsAndComplete(OK, session, NetErrorDetails());
    // `this` is deleted.
  }

  base::WeakPtr<Job> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // QuicSessionAttempt::Delegate implementation.
  QuicSessionPool* GetQuicSessionPool() override { return manager_->pool_; }
  const QuicSessionAliasKey& GetKey() override { return key_; }
  const NetLogWithSource& GetNetLog() override { return net_log_; }

 private:
  void OnAttemptComplete(QuicSessionAttempt* raw_attempt, int rv) {
    auto it = attempts_.find(raw_attempt);
    CHECK(it != attempts_.end());

    NetErrorDetails error_details;
    if (rv == OK) {
      QuicChromiumClientSession* session = raw_attempt->session();
      // Remove the successful attempt before cancelling the others. Its result
      // may be an already-active pooled session and must not be closed by
      // CancelAttempts().
      attempts_.erase(it);
      NotifyRequestsAndComplete(rv, session, std::move(error_details));
      return;
    }

    raw_attempt->PopulateNetErrorDetails(&error_details);
    attempts_.erase(it);
    if (!attempts_.empty()) {
      // Wait for other attempts to complete.
      return;
    }

    NotifyRequestsAndComplete(rv, /*session=*/nullptr,
                              std::move(error_details));
  }

  void AddRequest(QuicSessionAttemptRequest* request) {
    auto [_, inserted] = requests_.insert(request);
    CHECK(inserted);
  }

  QuicSessionAttempt* FindAttempt(const QuicEndpoint& endpoint) {
    auto it = std::ranges::find_if(attempts_, [&](const auto& attempt) {
      return attempt->quic_version() == endpoint.quic_version &&
             attempt->ip_endpoint() == endpoint.ip_endpoint &&
             attempt->metadata() == endpoint.metadata;
    });
    return it == attempts_.end() ? nullptr : it->get();
  }

  // Notifies all requests that the job is complete.
  void NotifyRequests(int rv,
                      QuicChromiumClientSession* session,
                      NetErrorDetails error_details) {
    notifying_requests_ = true;
    // Cancel other attempts.
    CancelAttempts();

    base::WeakPtr<Job> weak_this = GetWeakPtr();
    while (!requests_.empty()) {
      raw_ptr<QuicSessionAttemptRequest> request =
          requests_.extract(requests_.begin()).value();
      // Use ExtractAsDangling() because `request` may delete itself.
      request.ExtractAsDangling()->Complete(rv, session, error_details);
      if (!weak_this) {
        return;
      }
    }
    CHECK(requests_.empty());
  }

  void CancelAttempts() {
    for (const auto& attempt : attempts_) {
      attempt->Cancel();
    }
    attempts_.clear();
  }

  void NotifyRequestsAndComplete(int rv,
                                 QuicChromiumClientSession* session,
                                 NetErrorDetails error_details) {
    manager_->OnJobCompleting(this);
    base::WeakPtr<Job> weak_this = GetWeakPtr();
    NotifyRequests(rv, session, std::move(error_details));
    if (!weak_this) {
      return;
    }
    manager_->OnJobComplete(this);
    // `this` is deleted.
  }

  raw_ptr<QuicSessionAttemptManager> manager_;
  QuicSessionAliasKey key_;

  NetLogWithSource net_log_;

  // Set when `NotifyRequests()` starts invoking request completion callbacks
  // and never reset: it prevents `RemoveRequest()` from deleting `this` via
  // `OnJobComplete()` while the notification loop is still on the stack, and
  // a `Job` is always destroyed right after `NotifyRequests()` returns.
  // Deliberately not a `base::AutoReset`, which would write to `this` if a
  // callback destroys the `Job`.
  bool notifying_requests_ = false;
  std::set<raw_ptr<QuicSessionAttemptRequest>> requests_;

  base::flat_set<std::unique_ptr<QuicSessionAttempt>, base::UniquePtrComparator>
      attempts_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

QuicSessionAttemptManager::QuicSessionAttemptManager(QuicSessionPool* pool)
    : pool_(pool) {}

QuicSessionAttemptManager::~QuicSessionAttemptManager() {
  for (auto& [_, job] : active_jobs_) {
    completing_jobs_.insert(std::move(job));
  }
  active_jobs_.clear();
  while (!completing_jobs_.empty()) {
    Job* job = completing_jobs_.begin()->get();
    job->Abort();
    completing_jobs_.erase(job);
  }
  CHECK(active_jobs_.empty());
}

std::unique_ptr<QuicSessionAttemptRequest>
QuicSessionAttemptManager::CreateRequest(QuicSessionAliasKey key) {
  return base::WrapUnique(new QuicSessionAttemptRequest(this, std::move(key)));
}

int QuicSessionAttemptManager::RequestSession(
    QuicSessionAttemptRequest* request,
    QuicEndpoint endpoint,
    int cert_verify_flags,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    std::optional<ResolutionDetails> dns_resolution_details,
    bool use_dns_aliases,
    std::set<std::string> dns_aliases,
    MultiplexedSessionCreationInitiator session_creation_initiator,
    std::optional<ConnectionManagementConfig> connection_management_config,
    const NetLogWithSource& net_log) {
  auto it = active_jobs_.find(request->key_);
  if (it == active_jobs_.end()) {
    it = active_jobs_
             .try_emplace(request->key_,
                          std::make_unique<Job>(this, request->key_, net_log))
             .first;
  }

  return it->second->MaybeAttemptEndpoint(
      request, endpoint, cert_verify_flags, dns_resolution_start_time,
      dns_resolution_end_time, std::move(dns_resolution_details),
      use_dns_aliases, std::move(dns_aliases), session_creation_initiator,
      std::move(connection_management_config));
}

void QuicSessionAttemptManager::RemoveRequest(
    QuicSessionAttemptRequest* request) {
  auto it = active_jobs_.find(request->key_);
  if (it != active_jobs_.end() && it->second->HasRequest(request)) {
    it->second->RemoveRequest(request);
    return;
  }
  for (const auto& job : completing_jobs_) {
    if (job->HasRequest(request)) {
      job->RemoveRequest(request);
      return;
    }
  }
}

void QuicSessionAttemptManager::OnOriginFrame(
    QuicChromiumClientSession* session) {
  // Collect jobs that can be completed with `session` and then notify them
  // later to avoid erasing jobs during the loop.
  std::vector<base::WeakPtr<Job>> matched_jobs;
  for (auto& [key, job] : active_jobs_) {
    if (pool_->CanWaiveIpMatching(key.destination(), session) &&
        session->CanPool(key.session_key().host(), key.session_key())) {
      matched_jobs.push_back(job->GetWeakPtr());
    }
  }

  for (base::WeakPtr<Job>& job : matched_jobs) {
    if (job) {
      job->OnOriginFrameMatched(session);
      // `job` was removed from `active_jobs_` and it was deleted.
    }
  }
}

void QuicSessionAttemptManager::OnJobCompleting(Job* job) {
  auto it = active_jobs_.find(job->GetKey());
  CHECK(it != active_jobs_.end());
  CHECK_EQ(it->second.get(), job);
  std::unique_ptr<Job> owned_job = std::move(it->second);
  active_jobs_.erase(it);
  auto [_, inserted] = completing_jobs_.insert(std::move(owned_job));
  CHECK(inserted);
}

void QuicSessionAttemptManager::OnJobComplete(Job* job) {
  auto completing_it = completing_jobs_.find(job);
  if (completing_it != completing_jobs_.end()) {
    completing_jobs_.erase(completing_it);
    return;
  }
  auto it = active_jobs_.find(job->GetKey());
  CHECK(it != active_jobs_.end());
  active_jobs_.erase(it);
}

}  // namespace net
