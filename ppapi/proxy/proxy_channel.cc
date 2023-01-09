// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_channel.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sender.h"

#if BUILDFLAG(IS_NACL)
#include <unistd.h>
#endif

namespace ppapi {
namespace proxy {

ProxyChannel::ProxyChannel()
    : delegate_(NULL),
      peer_pid_(base::kNullProcessId),
      test_sink_(NULL) {
}

ProxyChannel::~ProxyChannel() {
  DVLOG(1) << "ProxyChannel::~ProxyChannel()";
}

bool ProxyChannel::InitWithChannel(
    Delegate* delegate,
    base::ProcessId peer_pid,
    const IPC::ChannelHandle& channel_handle,
    bool is_client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  delegate_ = delegate;
  peer_pid_ = peer_pid;
  IPC::Channel::Mode mode = is_client
      ? IPC::Channel::MODE_CLIENT
      : IPC::Channel::MODE_SERVER;
  DCHECK(task_runner->BelongsToCurrentThread());
  channel_ = IPC::SyncChannel::Create(channel_handle, mode, this,
                                      delegate->GetIPCTaskRunner(), task_runner,
                                      true, delegate->GetShutdownEvent());
  return true;
}

void ProxyChannel::InitWithTestSink(IPC::Sender* sender) {
  DCHECK(!test_sink_);
  test_sink_ = sender;
#if !BUILDFLAG(IS_NACL)
  peer_pid_ = base::GetCurrentProcId();
#endif
}

void ProxyChannel::OnChannelError() {
  channel_.reset();
}

IPC::PlatformFileForTransit ProxyChannel::ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) {
  // Channel could be closed if the plugin crashes.
  if (!channel_.get()) {
    if (should_close_source) {
      base::File file_closer(handle);
    }
    return IPC::InvalidPlatformFileForTransit();
  }
  DCHECK(peer_pid_ != base::kNullProcessId);
  return delegate_->ShareHandleWithRemote(handle, peer_pid_,
                                          should_close_source);
}

base::UnsafeSharedMemoryRegion
ProxyChannel::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region) {
  if (!channel_.get())
    return base::UnsafeSharedMemoryRegion();

  DCHECK(peer_pid_ != base::kNullProcessId);
  return delegate_->ShareUnsafeSharedMemoryRegionWithRemote(region, peer_pid_);
}

base::ReadOnlySharedMemoryRegion
ProxyChannel::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region) {
  if (!channel_.get())
    return base::ReadOnlySharedMemoryRegion();

  DCHECK(peer_pid_ != base::kNullProcessId);
  return delegate_->ShareReadOnlySharedMemoryRegionWithRemote(region,
                                                              peer_pid_);
}

bool ProxyChannel::Send(IPC::Message* msg) {
  if (test_sink_)
    return test_sink_->Send(msg);
  if (channel_.get())
    return channel_->Send(msg);

  // Remote side crashed, drop this message.
  delete msg;
  return false;
}

}  // namespace proxy
}  // namespace ppapi
