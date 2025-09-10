// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/atomic_sequence_num.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"

namespace IPC {

namespace {

// Global atomic used to guarantee channel IDs are unique.
base::AtomicSequenceNumber g_last_id;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

int g_global_pid = 0;

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

// static
constexpr size_t Channel::kMaximumMessageSize;

// static
std::unique_ptr<Channel> Channel::Create(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return base::WrapUnique(new ChannelMojo(std::move(handle), mode, listener,
                                          ipc_task_runner, proxy_task_runner));
}

// static
std::string Channel::GenerateUniqueRandomChannelID() {
  // Note: the string must start with the current process id, this is how
  // some child processes determine the pid of the parent.
  //
  // This is composed of a unique incremental identifier, the process ID of
  // the creator, an identifier for the child instance, and a strong random
  // component. The strong random component prevents other processes from
  // hijacking or squatting on predictable channel names.
  int process_id = base::GetCurrentProcId();
  return base::StringPrintf("%d.%u.%d",
      process_id,
      g_last_id.GetNext(),
      base::RandInt(0, std::numeric_limits<int32_t>::max()));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

void Channel::WillConnect() {
  did_start_connect_ = true;
}

}  // namespace IPC
