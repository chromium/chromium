// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_
#define CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_

#include <stddef.h>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

namespace content {

// Tracks a single "bucket" of pending handshakes. For frames and dedicated
// workers there is one object per Page. For shared and service workers there is
// one per profile.
class WebTransportThrottleContext final : public base::SupportsUserData::Data {
 public:
  static constexpr int kMaxPendingSessions = 64;

  enum class CheckResult {
    kContinue,
    kTooManyPendingSessions,
  };

  WebTransportThrottleContext();
  WebTransportThrottleContext(const WebTransportThrottleContext&) = delete;
  WebTransportThrottleContext& operator=(const WebTransportThrottleContext&) =
      delete;
  ~WebTransportThrottleContext() override;

  // Reports if a new handshake can be permitted to start. kContinue means it
  // can start immediately, and kTooManyPendingSessions means it should be
  // rejected immediately.
  CheckResult CheckThrottle();

  // Records the start of a WebTransport handshake.
  void OnCreateWebTransport();

  // Records the successful end of a WebTransport handshake.
  void OnHandshakeEstablished();

  // Records a WebTransport handshake failure.
  void OnHandshakeFailed();

  base::WeakPtr<WebTransportThrottleContext> GetWeakPtr();

 private:
  int pending_session_count_ = 0;

  base::WeakPtrFactory<WebTransportThrottleContext> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBTRANSPORT_WEB_TRANSPORT_THROTTLE_CONTEXT_H_
