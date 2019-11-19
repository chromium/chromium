// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"

#include "build/build_config.h"
#include "content/child/child_process.h"
#include "ipc/ipc_platform_file.h"

namespace content {

PepperProxyChannelDelegateImpl::~PepperProxyChannelDelegateImpl() {}

base::SingleThreadTaskRunner*
PepperProxyChannelDelegateImpl::GetIPCTaskRunner() {
  // This is called only in the renderer so we know we have a child process.
  DCHECK(ChildProcess::current()) << "Must be in the renderer.";
  return ChildProcess::current()->io_task_runner();
}

base::WaitableEvent* PepperProxyChannelDelegateImpl::GetShutdownEvent() {
  DCHECK(ChildProcess::current()) << "Must be in the renderer.";
  return ChildProcess::current()->GetShutDownEvent();
}

IPC::PlatformFileForTransit
PepperProxyChannelDelegateImpl::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId remote_pid,
    bool should_close_source) {
  return IPC::GetPlatformFileForTransit(handle, should_close_source);
}

base::UnsafeSharedMemoryRegion
PepperProxyChannelDelegateImpl::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  return region.Duplicate();
}

base::ReadOnlySharedMemoryRegion
PepperProxyChannelDelegateImpl::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region,
    base::ProcessId remote_pid) {
  return region.Duplicate();
}

}  // namespace content
