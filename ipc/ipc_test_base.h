// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_BASE_H_
#define IPC_IPC_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/multiprocess_test_helper.h"

class IPCChannelMojoTestBase : public testing::Test {
 public:
  IPCChannelMojoTestBase();

  IPCChannelMojoTestBase(const IPCChannelMojoTestBase&) = delete;
  IPCChannelMojoTestBase& operator=(const IPCChannelMojoTestBase&) = delete;

  ~IPCChannelMojoTestBase() override;

  void Init(const std::string& test_client_name);

  bool WaitForClientShutdown();

  void TearDown() override;

  void CreateChannel(IPC::Listener* listener);

  bool ConnectChannel();

  void DestroyChannel();

  IPC::Sender* sender() { return channel(); }
  IPC::Channel* channel() { return channel_.get(); }
  const base::Process& client_process() const { return helper_.test_child(); }

 protected:
  mojo::ScopedMessagePipeHandle TakeHandle();

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  mojo::ScopedMessagePipeHandle handle_;
  mojo::core::test::MultiprocessTestHelper helper_;

  std::unique_ptr<IPC::Channel> channel_;
};

class IpcChannelMojoTestClient {
 public:
  IpcChannelMojoTestClient();
  ~IpcChannelMojoTestClient();

  void Init(mojo::ScopedMessagePipeHandle handle);

  void Connect(IPC::Listener* listener);

  void Close();

  IPC::Channel* channel() const { return channel_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  mojo::ScopedMessagePipeHandle handle_;
  std::unique_ptr<IPC::Channel> channel_;
};

// Use this to declare the client side for tests using IPCChannelMojoTestBase
// when a custom test fixture class is required in the client. |test_base| must
// be derived from IpcChannelMojoTestClient.
#define DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(client_name,   \
                                                                test_base)     \
  class client_name##_MainFixture : public test_base {                         \
   public:                                                                     \
    void Main();                                                               \
  };                                                                           \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                           \
      client_name##TestChildMain,                                              \
      ::mojo::core::test::MultiprocessTestHelper::ChildSetup) {                \
    client_name##_MainFixture test;                                            \
    test.Init(                                                                 \
        std::move(mojo::core::test::MultiprocessTestHelper::primordial_pipe)); \
    test.Main();                                                               \
    return (::testing::Test::HasFatalFailure() ||                              \
            ::testing::Test::HasNonfatalFailure())                             \
               ? 1                                                             \
               : 0;                                                            \
  }                                                                            \
  void client_name##_MainFixture::Main()

// Use this to declare the client side for tests using IPCChannelMojoTestBase.
#define DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(client_name)   \
  DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE( \
      client_name, IpcChannelMojoTestClient)

#endif  // IPC_IPC_TEST_BASE_H_
