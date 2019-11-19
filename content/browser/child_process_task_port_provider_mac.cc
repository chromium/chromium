// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_task_port_provider_mac.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/common/child_process.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace content {

ChildProcessTaskPortProvider* ChildProcessTaskPortProvider::GetInstance() {
  static base::NoDestructor<ChildProcessTaskPortProvider> provider;
  return provider.get();
}

void ChildProcessTaskPortProvider::OnChildProcessLaunched(
    base::ProcessHandle pid,
    mojom::ChildProcess* child_process) {
  child_process->GetTaskPort(
      base::BindOnce(&ChildProcessTaskPortProvider::OnTaskPortReceived,
                     base::Unretained(this), pid));
}

mach_port_t ChildProcessTaskPortProvider::TaskForPid(
    base::ProcessHandle pid) const {
  base::AutoLock lock(lock_);
  PidToTaskPortMap::const_iterator it = pid_to_task_port_.find(pid);
  if (it == pid_to_task_port_.end())
    return MACH_PORT_NULL;
  return it->second.get();
}

ChildProcessTaskPortProvider::ChildProcessTaskPortProvider() {
  CHECK(base::mac::CreateMachPort(&notification_port_, nullptr));

  const std::string dispatch_name = base::StringPrintf(
      "%s.ChildProcessTaskPortProvider.%p", base::mac::BaseBundleID(), this);
  notification_source_ = std::make_unique<base::DispatchSourceMach>(
      dispatch_name.c_str(), notification_port_.get(), ^{
        OnTaskPortDied();
      });
  notification_source_->Resume();
}

ChildProcessTaskPortProvider::~ChildProcessTaskPortProvider() {}

void ChildProcessTaskPortProvider::OnTaskPortReceived(
    base::ProcessHandle pid,
    mojo::ScopedHandle task_port) {
  base::mac::ScopedMachSendRight port;
  if (mojo::UnwrapMachPort(
          std::move(task_port),
          base::mac::ScopedMachSendRight::Receiver(port).get()) !=
      MOJO_RESULT_OK) {
    DLOG(ERROR) << "Failed to unwrap task port for pid " << pid;
    return;
  }

  // Request a notification from the kernel for when the port becomes a dead
  // name, indicating that the process has died.
  base::mac::ScopedMachSendRight previous;
  kern_return_t kr = mach_port_request_notification(
      mach_task_self(), port.get(), MACH_NOTIFY_DEAD_NAME, 0,
      notification_port_.get(), MACH_MSG_TYPE_MAKE_SEND_ONCE,
      base::mac::ScopedMachSendRight::Receiver(previous).get());
  if (kr != KERN_SUCCESS) {
    // If the argument was invalid, the process is likely already dead.
    MACH_DVLOG(1, kr) << "mach_port_request_notification";
    return;
  }

  DVLOG(1) << "Received task port for PID=" << pid
           << ", port name=" << port.get();

  {
    base::AutoLock lock(lock_);
    auto it = pid_to_task_port_.find(pid);
    if (it == pid_to_task_port_.end()) {
      pid_to_task_port_.emplace(pid, std::move(port));
    } else {
      // If a task port already exists for the PID, then reset it if the port
      // is of a different name. The port name may be the same when running in
      // single-process mode, tests, or if the PID is reused and this races the
      // DEAD_NAME notification. Self-reseting is not allowed on ScopedGeneric,
      // so test for that first.
      it->second.swap(port);
    }
  }

  NotifyObservers(pid);
}

void ChildProcessTaskPortProvider::OnTaskPortDied() {
  mach_dead_name_notification_t notification{};
  kern_return_t kr =
      mach_msg(&notification.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
               sizeof(notification), notification_port_.get(), /*timeout=*/0,
               MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return;
  }

  // A NOTIFY_SEND_ONCE might be delivered from the send-once right allocated
  // via mach_port_request_notification().
  if (notification.not_header.msgh_id != MACH_NOTIFY_DEAD_NAME)
    return;

  // Release the DEAD_NAME right.
  base::mac::ScopedMachSendRight dead_port(notification.not_port);

  base::AutoLock lock(lock_);
  base::EraseIf(pid_to_task_port_, [&dead_port](const auto& pair) {
    if (pair.second.get() == dead_port.get()) {
      DVLOG(1) << "Task died, PID=" << pair.first
               << ", task port name=" << dead_port.get();
      return true;
    }
    return false;
  });
}

}  // namespace content
