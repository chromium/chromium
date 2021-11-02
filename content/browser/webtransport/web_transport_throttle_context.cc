// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/web_transport_throttle_context.h"

#include "base/logging.h"

namespace content {

WebTransportThrottleContext::WebTransportThrottleContext() = default;
WebTransportThrottleContext::~WebTransportThrottleContext() = default;

WebTransportThrottleContext::CheckResult
WebTransportThrottleContext::CheckThrottle() {
  DVLOG(1) << "WebTransportThrottleContext::CheckThrottle() this=" << this
           << " pending_session_count_=" << pending_session_count_;
  return pending_session_count_ < kMaxPendingSessions
             ? CheckResult::kContinue
             : CheckResult::kTooManyPendingSessions;
}

void WebTransportThrottleContext::OnCreateWebTransport() {
  DVLOG(1) << "WebTransportThrottleContext::OnCreateWebTransport() this="
           << this << " pending_session_count_= " << pending_session_count_;
  DCHECK_LT(pending_session_count_, kMaxPendingSessions);
  ++pending_session_count_;
}

void WebTransportThrottleContext::OnHandshakeEstablished() {
  DVLOG(1) << "WebTransportThrottleContext::OnHandshakeEstablished() this="
           << this << " pending_session_count_= " << pending_session_count_;
  DCHECK_GT(pending_session_count_, 0);
  --pending_session_count_;
}

void WebTransportThrottleContext::OnHandshakeFailed() {
  DVLOG(1) << "WebTransportThrottleContext::OnHandshakeFailed() this=" << this
           << " pending_session_count_= " << pending_session_count_;
  DCHECK_GT(pending_session_count_, 0);
  --pending_session_count_;
}

base::WeakPtr<WebTransportThrottleContext>
WebTransportThrottleContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
