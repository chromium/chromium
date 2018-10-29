// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mach_broker_mac.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

namespace content {

MachBroker* MachBroker::GetInstance() {
  return base::Singleton<MachBroker,
                         base::LeakySingletonTraits<MachBroker>>::get();
}

base::Lock& MachBroker::GetLock() {
  return broker_.GetLock();
}

void MachBroker::EnsureRunning() {
  GetLock().AssertAcquired();

  if (initialized_)
    return;

  // Do not attempt to reinitialize in the event of failure.
  initialized_ = true;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&MachBroker::RegisterNotifications, base::Unretained(this)));

  if (!broker_.Init()) {
    LOG(ERROR) << "Failed to initialize the MachListenerThreadDelegate";
  }
}

void MachBroker::AddPlaceholderForPid(base::ProcessHandle pid,
                                      int child_process_id) {
  GetLock().AssertAcquired();

  broker_.AddPlaceholderForPid(pid);
  child_process_id_map_[child_process_id] = pid;
}

mach_port_t MachBroker::TaskForPid(base::ProcessHandle pid) const {
  return broker_.TaskForPid(pid);
}

void MachBroker::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::BrowserChildProcessCrashed(
    const ChildProcessData& data,
    const ChildProcessTerminationInfo& info) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::RenderProcessExited(RenderProcessHost* host,
                                     const ChildProcessTerminationInfo& info) {
  InvalidateChildProcessId(host->GetID());
}

void MachBroker::RenderProcessHostDestroyed(RenderProcessHost* host) {
  InvalidateChildProcessId(host->GetID());
}

// static
std::string MachBroker::GetMachPortName() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool is_child = command_line->HasSwitch(switches::kProcessType);
  return base::MachPortBroker::GetMachPortName(kMachBootstrapName, is_child);
}

MachBroker::MachBroker() : initialized_(false), broker_(kMachBootstrapName) {
  broker_.AddObserver(this);
}

MachBroker::~MachBroker() {
  broker_.RemoveObserver(this);
}

void MachBroker::OnReceivedTaskPort(base::ProcessHandle process) {
  NotifyObservers(process);
}

void MachBroker::InvalidateChildProcessId(int child_process_id) {
  base::AutoLock lock(GetLock());
  MachBroker::ChildProcessIdMap::iterator it =
      child_process_id_map_.find(child_process_id);
  if (it == child_process_id_map_.end())
    return;

  broker_.InvalidatePid(it->second);
  child_process_id_map_.erase(it);
}

void MachBroker::RegisterNotifications() {
  // No corresponding StopObservingBrowserChildProcesses,
  // we leak this singleton.
  BrowserChildProcessObserver::Add(this);
}

}  // namespace content
