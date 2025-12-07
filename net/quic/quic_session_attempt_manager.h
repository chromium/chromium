// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_ATTEMPT_MANAGER_H_
#define NET_QUIC_QUIC_SESSION_ATTEMPT_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/reconnect_notifier.h"
#include "net/quic/quic_endpoint.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_pool.h"

namespace net {

class QuicChromiumClientSession;
class QuicSessionAttemptRequest;
class NetLogWithSource;

// Manages all in-flight QUIC session attempts. For each QuicSessionAliasKey
// that a client has requested, there can be at most one active
// QuicSessionAttemptManager::Job. A Job manages all attempts for the
// QuicSessionAliasKey (e.g. to different IP addresses) and all clients waiting
// for the result.
//
// The relationship between the manager, jobs, requests, and attempts is as
// follows:
//
//        +-------------- QuicSessionAttemptManager -------------+
//        |                         |                            |
//       Job                       Job                          Job
//    (for Key1)                (for Key2)                   (for KeyX)
//    /       \                 /       \                    /       \
// Requests   Attempts       Requests   Attempts         Requests   Attempts
//    |          |              |          |                |          |
// Request... Attempt...    Request... Attempt...        Request... Attempt...
// (client A) (endpoint 1)  (client C) (endpoint 3)      (client X) (endpoint X)
// (client B) (endpoint 2)             (endpoint 4)
//
// Owned by the QuicSessionPool.
class NET_EXPORT_PRIVATE QuicSessionAttemptManager {
 public:
  explicit QuicSessionAttemptManager(QuicSessionPool* pool);

  ~QuicSessionAttemptManager();

  // Creates a new QuicSessionAttemptRequest for the given key.
  std::unique_ptr<QuicSessionAttemptRequest> CreateRequest(
      QuicSessionAliasKey key);

  // Called by QuicSessionAttemptRequest to request a session. See
  // QuicSessionAttemptRequest for more details.
  int RequestSession(
      QuicSessionAttemptRequest* request,
      QuicEndpoint endpoint,
      int cert_verify_flags,
      base::TimeTicks dns_resolution_start_time,
      base::TimeTicks dns_resolution_end_time,
      bool use_dns_aliases,
      std::set<std::string> dns_aliases,
      MultiplexedSessionCreationInitiator session_creation_initiator,
      std::optional<ConnectionManagementConfig> connection_management_config,
      const NetLogWithSource& net_log);

  // Called by QuicSessionAttemptRequest to remove itself from the manager.
  void RemoveRequest(QuicSessionAttemptRequest* request);

  // Called when `session` received an HTTP/3 Origin frame. Checks if `session`
  // can be used to satisfy any active jobs. All jobs that can be satisfied by
  // `session` are completed successfully.
  void OnOriginFrame(QuicChromiumClientSession* session);

  bool HasActiveJobForTesting(const QuicSessionAliasKey& key) const {
    return active_jobs_.find(key) != active_jobs_.end();
  }

 private:
  class Job;

  // Called by Job when the last request is completed.
  void OnJobComplete(Job* job);

  const raw_ptr<QuicSessionPool> pool_;

  base::flat_map<QuicSessionAliasKey, std::unique_ptr<Job>> active_jobs_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_ATTEMPT_MANAGER_H_
