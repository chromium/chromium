// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/tracing_process_set_monitor.h"

#include "content/public/browser/render_process_host.h"

namespace content {

// static
std::unique_ptr<TracingProcessSetMonitor> TracingProcessSetMonitor::Start(
    DevToolsSession& root_session,
    ProcessAddedCallback callback) {
  return base::WrapUnique(
      new TracingProcessSetMonitor(root_session, std::move(callback)));
}

void TracingProcessSetMonitor::SessionAttached(DevToolsSession& session) {
  CHECK(session.GetRootSession() == &*root_session_);
  auto* const host =
      static_cast<DevToolsAgentHostImpl*>(session.GetAgentHost());
  CHECK(host);
  if (bool inserted = hosts_.insert(host).second; inserted) {
    MaybeAddProcess(host);
  }
}

void TracingProcessSetMonitor::DevToolsAgentHostDetached(
    DevToolsAgentHost* host) {
  hosts_.erase(host);
}

void TracingProcessSetMonitor::DevToolsAgentHostDestroyed(
    DevToolsAgentHost* host) {
  hosts_.erase(host);
}

void TracingProcessSetMonitor::DevToolsAgentHostProcessChanged(
    DevToolsAgentHost* host) {
  if (!hosts_.contains(host)) {
    return;
  }
  MaybeAddProcess(host);
}

void TracingProcessSetMonitor::MaybeAddProcess(DevToolsAgentHost* host) {
  base::ProcessId pid =
      static_cast<DevToolsAgentHostImpl*>(host)->GetProcessId();
  if (pid == base::kNullProcessId) {
    return;
  }
  AddProcess(pid);
}

void TracingProcessSetMonitor::AddProcess(base::ProcessId pid) {
  const bool inserted = known_pids_.insert(pid).second;
  if (inserted && !in_init_) {
    process_added_callback_.Run(pid);
  }
}

TracingProcessSetMonitor::TracingProcessSetMonitor(
    DevToolsSession& root_session,
    ProcessAddedCallback callback)
    : root_session_(root_session),
      process_added_callback_(std::move(callback)) {
  base::AutoReset<bool> auto_reset(&in_init_, true);
  session_observation_.Observe(&root_session);
  DevToolsAgentHost::AddObserver(this);
  SessionAttached(root_session);
}

TracingProcessSetMonitor::~TracingProcessSetMonitor() {
  DevToolsAgentHost::RemoveObserver(this);
}

}  // namespace content
