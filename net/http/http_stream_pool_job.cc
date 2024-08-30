// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"

namespace net {

HttpStreamPool::Job::Job(Delegate* delegate, AttemptManager* attempt_manager)
    : delegate_(delegate), attempt_manager_(attempt_manager) {}

HttpStreamPool::Job::~Job() = default;

LoadState HttpStreamPool::Job::GetLoadState() const {
  CHECK(attempt_manager_);
  return attempt_manager_->GetLoadState();
}

void HttpStreamPool::Job::SetPriority(RequestPriority priority) {
  CHECK(attempt_manager_);
  attempt_manager_->SetJobPriority(this, priority);
}

void HttpStreamPool::Job::NotifyAttemptManagerOfCompletion() {
  CHECK(attempt_manager_);
  // `attempt_manager_` may be deleted after this call.
  attempt_manager_.ExtractAsDangling()->OnJobComplete(this);
}

void HttpStreamPool::Job::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  for (const auto& attempt : attempts) {
    connection_attempts_.emplace_back(attempt);
  }
}

void HttpStreamPool::Job::OnStreamReady(std::unique_ptr<HttpStream> stream,
                                        NextProto negotiated_protocol) {
  delegate_->OnStreamReady(this, std::move(stream), negotiated_protocol);
}

void HttpStreamPool::Job::OnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  delegate_->OnStreamFailed(this, status, net_error_details,
                            resolve_error_info);
}

void HttpStreamPool::Job::OnCertificateError(int status,
                                             const SSLInfo& ssl_info) {
  delegate_->OnCertificateError(this, status, ssl_info);
}

void HttpStreamPool::Job::OnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  delegate_->OnNeedsClientAuth(this, cert_info);
}

}  // namespace net
