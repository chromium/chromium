// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {
int g_global_pid = 0;
}

// static
void Channel::SetGlobalPid(int pid) {
  g_global_pid = pid;
}

// static
int Channel::GetGlobalPid() {
  return g_global_pid;
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

Channel::~Channel() = default;

Channel::AssociatedInterfaceSupport* Channel::GetAssociatedInterfaceSupport() {
  return nullptr;
}

void Channel::Pause() {
  NOTREACHED();
}

void Channel::Unpause(bool flush) {
  NOTREACHED();
}

void Channel::Flush() {
  NOTREACHED();
}

void Channel::SetUrgentMessageObserver(UrgentMessageObserver* observer) {
  // Ignored for non-mojo channels.
}

void Channel::WillConnect() {
  did_start_connect_ = true;
}

}  // namespace IPC
