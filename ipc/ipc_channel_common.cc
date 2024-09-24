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

// static
std::unique_ptr<Channel> Channel::CreateClient(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
#if BUILDFLAG(IS_NACL)
  return Channel::Create(channel_handle, Channel::MODE_CLIENT, listener);
#else
  DCHECK(channel_handle.is_mojo_channel_handle());
  return ChannelMojo::Create(
      mojo::ScopedMessagePipeHandle(channel_handle.mojo_handle),
      Channel::MODE_CLIENT, listener, ipc_task_runner,
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif
}

// static
std::unique_ptr<Channel> Channel::CreateServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
#if BUILDFLAG(IS_NACL)
  return Channel::Create(channel_handle, Channel::MODE_SERVER, listener);
#else
  DCHECK(channel_handle.is_mojo_channel_handle());
  return ChannelMojo::Create(
      mojo::ScopedMessagePipeHandle(channel_handle.mojo_handle),
      Channel::MODE_SERVER, listener, ipc_task_runner,
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif
}

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
