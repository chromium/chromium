// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_attempt_request.h"

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_session_attempt_manager.h"

namespace net {

QuicSessionAttemptRequest::QuicSessionAttemptRequest(
    QuicSessionAttemptManager* manager,
    QuicSessionAliasKey key)
    : manager_(manager), key_(std::move(key)) {}

QuicSessionAttemptRequest::~QuicSessionAttemptRequest() {
  if (manager_ && callback_) {
    manager_->RemoveRequest(this);
  }
}

int QuicSessionAttemptRequest::RequestSession(
    QuicEndpoint endpoint,
    int cert_verify_flags,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    bool use_dns_aliases,
    std::set<std::string> dns_aliases,
    MultiplexedSessionCreationInitiator session_creation_initiator,
    std::optional<ConnectionManagementConfig> connection_management_config,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  int rv = manager_->RequestSession(
      this, std::move(endpoint), cert_verify_flags, dns_resolution_start_time,
      dns_resolution_end_time, use_dns_aliases, std::move(dns_aliases),
      session_creation_initiator, std::move(connection_management_config),
      net_log);
  if (rv == ERR_IO_PENDING) {
    CHECK(!completed_);
    callback_ = std::move(callback);
  } else {
    CHECK(completed_);
  }
  return rv;
}

void QuicSessionAttemptRequest::Complete(int rv,
                                         QuicChromiumClientSession* session,
                                         NetErrorDetails error_details) {
  CHECK(!completed_);
  completed_ = true;
  session_ = session;
  error_details_ = std::move(error_details);

  manager_ = nullptr;
  if (callback_) {
    std::move(callback_).Run(rv);
  }
}

}  // namespace net
