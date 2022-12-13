// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_factory.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(
      ChannelHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : handle_(handle), mode_(mode), ipc_task_runner_(ipc_task_runner) {}

  PlatformChannelFactory(const PlatformChannelFactory&) = delete;
  PlatformChannelFactory& operator=(const PlatformChannelFactory&) = delete;

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
#if BUILDFLAG(IS_NACL)
    return Channel::Create(handle_, mode_, listener);
#else
    DCHECK(handle_.is_mojo_channel_handle());
    return ChannelMojo::Create(
        mojo::ScopedMessagePipeHandle(handle_.mojo_handle), mode_, listener,
        ipc_task_runner_, base::SingleThreadTaskRunner::GetCurrentDefault());
#endif
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

 private:
  ChannelHandle handle_;
  Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
};

} // namespace

// static
std::unique_ptr<ChannelFactory> ChannelFactory::Create(
    const ChannelHandle& handle,
    Channel::Mode mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return std::make_unique<PlatformChannelFactory>(handle, mode,
                                                  ipc_task_runner);
}

}  // namespace IPC
