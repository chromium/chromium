// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_TRACING_PROCESS_SET_MONITOR_H_
#define CONTENT_BROWSER_DEVTOOLS_TRACING_PROCESS_SET_MONITOR_H_

#include <memory>
#include <unordered_set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "base/scoped_observation.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class TracingProcessSetMonitor : public DevToolsSession::ChildObserver,
                                 public DevToolsAgentHostObserver {
 public:
  using ProcessAddedCallback =
      base::RepeatingCallback<void(base::ProcessId pid)>;

  // |callback| will be invoked once per each new process added after start.
  // The process present at start time are not reported via the callback
  // and are available through |GetPids()| below.
  static std::unique_ptr<TracingProcessSetMonitor> Start(
      DevToolsSession& root_session,
      ProcessAddedCallback callback);

  TracingProcessSetMonitor(const TracingProcessSetMonitor& r) = delete;
  ~TracingProcessSetMonitor() override;

  const std::unordered_set<base::ProcessId>& GetPids() const {
    return known_pids_;
  }

  void AddProcess(base::ProcessId pid);

 private:
  TracingProcessSetMonitor(DevToolsSession& root_session,
                           ProcessAddedCallback callback);

  // DevToolsSession::ChildObserver methods.
  void SessionAttached(DevToolsSession& session) override;

  // DevToolsAgentHostObserver methods.
  void DevToolsAgentHostDetached(DevToolsAgentHost* host) override;
  void DevToolsAgentHostDestroyed(DevToolsAgentHost* host) override;
  void DevToolsAgentHostProcessChanged(DevToolsAgentHost* host) override;

  void MaybeAddProcess(DevToolsAgentHost* host);

  raw_ref<DevToolsSession> const root_session_;
  base::ScopedObservation<DevToolsSession, DevToolsSession::ChildObserver>
      session_observation_{this};
  const ProcessAddedCallback process_added_callback_;

  bool in_init_{false};
  std::unordered_set<raw_ptr<const DevToolsAgentHost, CtnExperimental>> hosts_;
  std::unordered_set<base::ProcessId> known_pids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_TRACING_PROCESS_SET_MONITOR_H_
