// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_ATTEMPT_REQUEST_H_
#define NET_QUIC_QUIC_SESSION_ATTEMPT_REQUEST_H_

#include <optional>
#include <set>
#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_pool.h"

namespace net {

class QuicSessionAttemptManager;

// Represents a request to attempt creation of a new QUIC session. This class
// is owned by the creator of the request. If the request is still pending when
// the request is destroyed, it will be cancelled.
class NET_EXPORT_PRIVATE QuicSessionAttemptRequest {
 public:
  QuicSessionAttemptRequest(const QuicSessionAttemptRequest&) = delete;
  QuicSessionAttemptRequest& operator=(const QuicSessionAttemptRequest&) =
      delete;

  ~QuicSessionAttemptRequest();

  // Requests a QUIC session. If the request is completed synchronously, returns
  // the result. If the request is completed asynchronously, returns
  // ERR_IO_PENDING and `callback` will be invoked later. See also
  // `QuicSessionAttempt`.
  int RequestSession(
      QuicEndpoint endpoint,
      int cert_verify_flags,
      base::TimeTicks dns_resolution_start_time,
      base::TimeTicks dns_resolution_end_time,
      bool use_dns_aliases,
      std::set<std::string> dns_aliases,
      MultiplexedSessionCreationInitiator session_creation_initiator,
      std::optional<ConnectionManagementConfig> connection_management_config,
      const NetLogWithSource& net_log,
      CompletionOnceCallback callback);

  const QuicSessionAliasKey& key() const { return key_; }

  // Returns the error details of the request. Populated only if the request is
  // failed. Only valid to call after the request is completed.
  const NetErrorDetails& error_details() const {
    CHECK(completed_);
    return error_details_;
  }

  // Returns the session of the request. Can be nullptr if the request is
  // failed. Only valid to call after the request is completed.
  raw_ptr<QuicChromiumClientSession> session() const {
    CHECK(completed_);
    return session_;
  }

 private:
  friend class QuicSessionAttemptManager;

  explicit QuicSessionAttemptRequest(QuicSessionAttemptManager* manager,
                                     QuicSessionAliasKey key);

  void Complete(int rv,
                QuicChromiumClientSession* session,
                NetErrorDetails error_details);

  raw_ptr<QuicSessionAttemptManager> manager_;
  const QuicSessionAliasKey key_;

  bool completed_ = false;
  CompletionOnceCallback callback_;

  NetErrorDetails error_details_;
  raw_ptr<QuicChromiumClientSession> session_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_ATTEMPT_REQUEST_H_
