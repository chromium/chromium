// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_test_base.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo.h"

IPCChannelMojoTestBase::IPCChannelMojoTestBase() = default;
IPCChannelMojoTestBase::~IPCChannelMojoTestBase() = default;

void IPCChannelMojoTestBase::Init(const std::string& test_client_name) {
  InitWithCustomMessageLoop(test_client_name,
                            std::make_unique<base::MessageLoop>());
}

void IPCChannelMojoTestBase::InitWithCustomMessageLoop(
    const std::string& test_client_name,
    std::unique_ptr<base::MessageLoop> message_loop) {
  handle_ = helper_.StartChild(test_client_name);
  message_loop_ = std::move(message_loop);
}

bool IPCChannelMojoTestBase::WaitForClientShutdown() {
  return helper_.WaitForChildTestShutdown();
}

void IPCChannelMojoTestBase::TearDown() {
  if (message_loop_)
    base::RunLoop().RunUntilIdle();
}

void IPCChannelMojoTestBase::CreateChannel(IPC::Listener* listener) {
  channel_ =
      IPC::ChannelMojo::Create(TakeHandle(), IPC::Channel::MODE_SERVER,
                               listener, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get(), nullptr);
}

bool IPCChannelMojoTestBase::ConnectChannel() {
  return channel_->Connect();
}

void IPCChannelMojoTestBase::DestroyChannel() {
  channel_.reset();
}

mojo::ScopedMessagePipeHandle IPCChannelMojoTestBase::TakeHandle() {
  return std::move(handle_);
}

IpcChannelMojoTestClient::IpcChannelMojoTestClient() = default;

IpcChannelMojoTestClient::~IpcChannelMojoTestClient() = default;

void IpcChannelMojoTestClient::Init(mojo::ScopedMessagePipeHandle handle) {
  handle_ = std::move(handle);
}

void IpcChannelMojoTestClient::Connect(IPC::Listener* listener) {
  channel_ =
      IPC::ChannelMojo::Create(std::move(handle_), IPC::Channel::MODE_CLIENT,
                               listener, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get(), nullptr);
  CHECK(channel_->Connect());
}

void IpcChannelMojoTestClient::Close() {
  channel_->Close();

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
}
