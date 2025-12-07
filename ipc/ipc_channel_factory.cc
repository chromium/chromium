// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_factory.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

namespace IPC {

namespace {

class PlatformChannelFactory : public ChannelFactory {
 public:
  PlatformChannelFactory(
      mojo::MessagePipeHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : handle_(handle), mode_(mode), ipc_task_runner_(ipc_task_runner) {}

  PlatformChannelFactory(const PlatformChannelFactory&) = delete;
  PlatformChannelFactory& operator=(const PlatformChannelFactory&) = delete;

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    DCHECK(handle_.is_valid());
    return Channel::Create(mojo::ScopedMessagePipeHandle(handle_), mode_,
                           listener, ipc_task_runner_,
                           base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

 private:
  mojo::MessagePipeHandle handle_;
  Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
};

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(
      mojo::ScopedMessagePipeHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner)
      : handle_(std::move(handle)),
        mode_(mode),
        ipc_task_runner_(ipc_task_runner),
        proxy_task_runner_(proxy_task_runner) {}

  MojoChannelFactory(const MojoChannelFactory&) = delete;
  MojoChannelFactory& operator=(const MojoChannelFactory&) = delete;

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    return Channel::Create(std::move(handle_), mode_, listener,
                           ipc_task_runner_, proxy_task_runner_);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

 private:
  mojo::ScopedMessagePipeHandle handle_;
  const Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
};

} // namespace

// static
std::unique_ptr<ChannelFactory> ChannelFactory::Create(
    const mojo::MessagePipeHandle& handle,
    Channel::Mode mode,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return std::make_unique<PlatformChannelFactory>(handle, mode,
                                                  ipc_task_runner);
}

// static
std::unique_ptr<ChannelFactory> ChannelFactory::CreateServerFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return std::make_unique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_SERVER, ipc_task_runner,
      proxy_task_runner);
}

// static
std::unique_ptr<ChannelFactory> ChannelFactory::CreateClientFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return std::make_unique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_CLIENT, ipc_task_runner,
      proxy_task_runner);
}

}  // namespace IPC
