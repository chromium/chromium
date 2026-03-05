// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_
#define CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_

#include "content/common/content_export.h"

namespace base {
class Process;
class TimeDelta;
}  // namespace base

namespace content {

// Starts the NetworkServiceListener if it is not already running. This prevents
// other processes from reusing the Network Service's process ID for a period
// after it is restarted, to help ensure that network::SocketBrokerImpl does not
// assign a socket to the wrong process.
CONTENT_EXPORT void EnsureNetworkServiceListenerStarted();

// Attempts to obtain a base::Process representing the current running network
// service. This might return an invalid base::Process if the network service
// has been restarted recently. It may also briefly return a base::Process for a
// previous incarnation of the network service due to delays in propagating
// updates to the UI thread. It's best to avoid needing this if you can. Must be
// called on the UI thread.
CONTENT_EXPORT base::Process GetNetworkServiceProcessForTesting();

// Allow tests to override how long the Network Service's process handle is kept
// alive after it goes away.
class CONTENT_EXPORT ScopedKeepOldProcessHandlePeriodForTesting {
 public:
  explicit ScopedKeepOldProcessHandlePeriodForTesting(base::TimeDelta duration);

  // Not copyable or assignable.
  ScopedKeepOldProcessHandlePeriodForTesting(
      const ScopedKeepOldProcessHandlePeriodForTesting&) = delete;
  ScopedKeepOldProcessHandlePeriodForTesting& operator=(
      const ScopedKeepOldProcessHandlePeriodForTesting&) = delete;

  ~ScopedKeepOldProcessHandlePeriodForTesting();
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_PROCESS_TRACKER_WIN_H_
