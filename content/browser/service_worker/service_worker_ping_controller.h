// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PING_CONTROLLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PING_CONTROLLER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

class ServiceWorkerVersion;

// A controller for periodically sending a ping to the worker to see if the
// worker is not stalling or is in a busy synchronous loop (possibly abusively).
class CONTENT_EXPORT ServiceWorkerPingController final {
 public:
  explicit ServiceWorkerPingController(ServiceWorkerVersion* version);
  ~ServiceWorkerPingController();

  void Activate();
  void Deactivate();
  void OnPongReceived();

  bool IsActivated() const;
  bool IsTimedOut() const;

  // Checks ping status. This is supposed to be called periodically.
  // This may call:
  // - version_->OnPingTimeout() if the worker hasn't reponded within a
  //   certain period.
  // - version_->PingWorker() if we're running ping timer and can send next
  //   ping.
  void CheckPingStatus();

  void SimulateTimeoutForTesting();

 private:
  void ClearLastPingTime();

  enum class PingState { kNotPinging, kPinging, kPingTimedOut };
  ServiceWorkerVersion* version_;  // Owns |this|.
  // The time the most recent ping was sent.
  base::TimeTicks last_ping_time_;
  PingState ping_state_ = PingState::kNotPinging;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPingController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PING_CONTROLLER_H_
