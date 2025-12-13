// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_attempt_manager.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
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

  ~Job() override {
    // Notify all pending requests that the job is aborted.
    if (!requests_.empty()) {
      NotifyRequests(ERR_ABORTED, /*session=*/nullptr, NetErrorDetails());
    }
  }

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

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
            dns_resolution_start_time, dns_resolution_end_time, use_dns_aliases,
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

    if (requests_.empty()) {
      manager_->OnJobComplete(this);
      // `this` is deleted.
    }
  }

  void OnOriginFrameMatched(QuicChromiumClientSession* session) {
    NotifyRequestsAndComplete(OK, session, NetErrorDetails());
    // `this` is deleted.
  }

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
    // Cancel other attempts.
    attempts_.clear();

    while (!requests_.empty()) {
      raw_ptr<QuicSessionAttemptRequest> request =
          requests_.extract(requests_.begin()).value();
      // Use ExtractAsDangling() because `request` may delete itself.
      request.ExtractAsDangling()->Complete(rv, session, error_details);
    }
    CHECK(requests_.empty());
  }

  void NotifyRequestsAndComplete(int rv,
                                 QuicChromiumClientSession* session,
                                 NetErrorDetails error_details) {
    NotifyRequests(rv, session, std::move(error_details));
    manager_->OnJobComplete(this);
    // `this` is deleted.
  }

  raw_ptr<QuicSessionAttemptManager> manager_;
  QuicSessionAliasKey key_;

  NetLogWithSource net_log_;

  std::set<raw_ptr<QuicSessionAttemptRequest>> requests_;

  base::flat_set<std::unique_ptr<QuicSessionAttempt>, base::UniquePtrComparator>
      attempts_;
};

QuicSessionAttemptManager::QuicSessionAttemptManager(QuicSessionPool* pool)
    : pool_(pool) {}

QuicSessionAttemptManager::~QuicSessionAttemptManager() {
  // Clear the active jobs, first moving out of the instance variable so that
  // calls to RemoveRequest for any pending requests do not cause recursion.
  base::flat_map<QuicSessionAliasKey, std::unique_ptr<Job>> active_jobs =
      std::move(active_jobs_);
  active_jobs.clear();
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
      dns_resolution_end_time, use_dns_aliases, std::move(dns_aliases),
      session_creation_initiator, std::move(connection_management_config));
}

void QuicSessionAttemptManager::RemoveRequest(
    QuicSessionAttemptRequest* request) {
  auto it = active_jobs_.find(request->key_);
  if (it == active_jobs_.end()) {
    return;
  }
  it->second->RemoveRequest(request);
}

void QuicSessionAttemptManager::OnOriginFrame(
    QuicChromiumClientSession* session) {
  // Collect jobs that can be completed with `session` and then notify them
  // later to avoid erasing jobs during the loop.
  std::vector<Job*> matched_jobs;
  for (auto& [key, job] : active_jobs_) {
    if (pool_->CanWaiveIpMatching(key.destination(), session) &&
        session->CanPool(key.session_key().host(), key.session_key())) {
      matched_jobs.push_back(job.get());
    }
  }

  for (auto job : matched_jobs) {
    job->OnOriginFrameMatched(session);
    // `job` was removed from `active_jobs_` and it was deleted.
  }
  matched_jobs.clear();
}

void QuicSessionAttemptManager::OnJobComplete(Job* job) {
  auto it = active_jobs_.find(job->GetKey());
  CHECK(it != active_jobs_.end());
  active_jobs_.erase(it);
}

}  // namespace net
