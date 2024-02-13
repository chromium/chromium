// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_task_port_provider_mac.h"

#include <map>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/common/child_process.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if BUILDFLAG(IS_MAC)
#include "content/common/mac/task_port_policy.h"
#endif

namespace content {

ChildProcessTaskPortProvider* ChildProcessTaskPortProvider::GetInstance() {
  static base::NoDestructor<ChildProcessTaskPortProvider> provider;
  return provider.get();
}

void ChildProcessTaskPortProvider::OnChildProcessLaunched(
    base::ProcessHandle process_handle,
    mojom::ChildProcess* child_process) {
  if (!ShouldRequestTaskPorts())
    return;

  child_process->GetTaskPort(
      base::BindOnce(&ChildProcessTaskPortProvider::OnTaskPortReceived,
                     base::Unretained(this), process_handle));
}

mach_port_t ChildProcessTaskPortProvider::TaskForHandle(
    base::ProcessHandle process_handle) const {
  base::AutoLock lock(lock_);
  auto it = handle_to_task_port_.find(process_handle);
  if (it == handle_to_task_port_.end()) {
    return MACH_PORT_NULL;
  }
  return it->second.get();
}

ChildProcessTaskPortProvider::ChildProcessTaskPortProvider() {
  if (!ShouldRequestTaskPorts()) {
    LOG(WARNING) << "AppleMobileFileIntegrity is disabled. The browser will "
                    "not collect child process task ports.";
    return;
  }

  CHECK(base::apple::CreateMachPort(&notification_port_, nullptr));

  const std::string dispatch_name = base::StringPrintf(
      "%s.ChildProcessTaskPortProvider.%p", base::apple::BaseBundleID(), this);
  notification_source_ = std::make_unique<base::apple::DispatchSourceMach>(
      dispatch_name.c_str(), notification_port_.get(), ^{
        OnTaskPortDied();
      });
  notification_source_->Resume();
}

ChildProcessTaskPortProvider::~ChildProcessTaskPortProvider() {}

bool ChildProcessTaskPortProvider::ShouldRequestTaskPorts() const {
#if BUILDFLAG(IS_MAC)
  // Set a crash key for the lifetime of the browser process to help debug
  // other failures.
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "amfi-status", base::debug::CrashKeySize::Size64);
  static const bool should_request_task_ports =
      [](base::debug::CrashKeyString* crash_key) -> bool {
    const MachTaskPortPolicy task_port_policy(GetMachTaskPortPolicy());
    bool allow_everything = task_port_policy.AmfiIsAllowEverything();
    base::debug::SetCrashKeyString(
        crash_key,
        base::StringPrintf("rv=%d status=0x%llx allow_everything=%d",
                           task_port_policy.amfi_status_retval,
                           task_port_policy.amfi_status, allow_everything));
    return !allow_everything;
  }(crash_key);
  return should_request_task_ports;
#else
  return true;
#endif
}

void ChildProcessTaskPortProvider::OnTaskPortReceived(
    base::ProcessHandle process_handle,
    mojo::PlatformHandle task_port) {
  DCHECK(ShouldRequestTaskPorts());
  if (!task_port.is_mach_send()) {
    DLOG(ERROR) << "Invalid handle received as task port for pid "
                << base::GetProcId(process_handle);
    return;
  }
  base::apple::ScopedMachSendRight port = task_port.TakeMachSendRight();

  // Request a notification from the kernel for when the port becomes a dead
  // name, indicating that the process has died.
  base::apple::ScopedMachSendRight previous;
  kern_return_t kr = mach_port_request_notification(
      mach_task_self(), port.get(), MACH_NOTIFY_DEAD_NAME, 0,
      notification_port_.get(), MACH_MSG_TYPE_MAKE_SEND_ONCE,
      base::apple::ScopedMachSendRight::Receiver(previous).get());
  if (kr != KERN_SUCCESS) {
    // If the argument was invalid, the process is likely already dead.
    MACH_DVLOG(1, kr) << "mach_port_request_notification";
    return;
  }

  DVLOG(1) << "Received task port for PID=" << base::GetProcId(process_handle)
           << ", port name=" << port.get();

  {
    base::AutoLock lock(lock_);
    auto it = handle_to_task_port_.find(process_handle);
    if (it == handle_to_task_port_.end()) {
      handle_to_task_port_.emplace(process_handle, std::move(port));
    } else {
      // If a task port already exists for the PID, then reset it if the port
      // is of a different name. The port name may be the same when running in
      // single-process mode, tests, or if the PID is reused and this races the
      // DEAD_NAME notification. Self-reseting is not allowed on ScopedGeneric,
      // so test for that first.
      if (it->second.get() != port.get())
        it->second = std::move(port);
    }
  }

  NotifyObservers(process_handle);
}

void ChildProcessTaskPortProvider::OnTaskPortDied() {
  DCHECK(ShouldRequestTaskPorts());

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
  base::apple::ScopedMachSendRight dead_port(notification.not_port);

  base::AutoLock lock(lock_);
  std::erase_if(handle_to_task_port_, [&dead_port](const auto& pair) {
    if (pair.second.get() == dead_port.get()) {
      DVLOG(1) << "Task died, PID=" << base::GetProcId(pair.first)
               << ", task port name=" << dead_port.get();
      return true;
    }
    return false;
  });
}

}  // namespace content
