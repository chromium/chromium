// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PROXY_CHANNEL_DELEGATE_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PROXY_CHANNEL_DELEGATE_IMPL_H_

#include "base/task/single_thread_task_runner.h"
#include "ppapi/proxy/proxy_channel.h"

namespace base {
class WaitableEvent;
}

namespace content {

class PepperProxyChannelDelegateImpl
    : public ppapi::proxy::ProxyChannel::Delegate {
 public:
  ~PepperProxyChannelDelegateImpl() override;

  // ProxyChannel::Delegate implementation.
  base::SingleThreadTaskRunner* GetIPCTaskRunner() override;
  base::WaitableEvent* GetShutdownEvent() override;
  IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      base::ProcessId remote_pid,
      bool should_close_source) override;
  base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region,
      base::ProcessId remote_pid) override;
  base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region,
      base::ProcessId remote_pid) override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PROXY_CHANNEL_DELEGATE_IMPL_H_
