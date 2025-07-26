// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "ipc/ipc_channel_proxy_unittest_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC


namespace {

void CreateRunLoopAndRun(raw_ptr<base::RunLoop>* run_loop_ptr) {
  base::RunLoop run_loop;
  *run_loop_ptr = &run_loop;
  run_loop.Run();
  *run_loop_ptr = nullptr;
}

class QuitListener : public IPC::Listener {
 public:
  QuitListener() = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(QuitListener, message)
      IPC_MESSAGE_HANDLER(WorkerMsg_Quit, OnQuit)
      IPC_MESSAGE_HANDLER(TestMsg_BadMessage, OnBadMessage)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnBadMessageReceived(const IPC::Message& message) override {
    bad_message_received_ = true;
  }

  void OnChannelError() override { CHECK(quit_message_received_); }

  void OnQuit() {
    quit_message_received_ = true;
    run_loop_->QuitWhenIdle();
  }

  void OnBadMessage(const BadType& bad_type) {
    // Should never be called since IPC wouldn't be deserialized correctly.
    NOTREACHED();
  }

  bool bad_message_received_ = false;
  bool quit_message_received_ = false;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

class ChannelReflectorListener : public IPC::Listener {
 public:
  ChannelReflectorListener() = default;

  void Init(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ChannelReflectorListener, message)
      IPC_MESSAGE_HANDLER(TestMsg_Bounce, OnTestBounce)
      IPC_MESSAGE_HANDLER(TestMsg_SendBadMessage, OnSendBadMessage)
      IPC_MESSAGE_HANDLER(AutomationMsg_Bounce, OnAutomationBounce)
      IPC_MESSAGE_HANDLER(WorkerMsg_Bounce, OnBounce)
      IPC_MESSAGE_HANDLER(WorkerMsg_Quit, OnQuit)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnTestBounce() {
    channel_->Send(new TestMsg_Bounce());
  }

  void OnSendBadMessage() {
    channel_->Send(new TestMsg_BadMessage(BadType()));
  }

  void OnAutomationBounce() { channel_->Send(new AutomationMsg_Bounce()); }

  void OnBounce() {
    channel_->Send(new WorkerMsg_Bounce());
  }

  void OnQuit() {
    channel_->Send(new WorkerMsg_Quit());
    run_loop_->QuitWhenIdle();
  }

  raw_ptr<base::RunLoop> run_loop_ = nullptr;

 private:
  raw_ptr<IPC::Channel> channel_ = nullptr;
};


class IPCChannelProxyTest : public IPCChannelMojoTestBase {
 public:
  IPCChannelProxyTest() = default;
  ~IPCChannelProxyTest() override = default;

  void SetUp() override {
    IPCChannelMojoTestBase::SetUp();

    Init("ChannelProxyClient");

    thread_ = std::make_unique<base::Thread>("ChannelProxyTestServerThread");
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    thread_->StartWithOptions(std::move(options));

    listener_ = std::make_unique<QuitListener>();
    channel_proxy_ = IPC::ChannelProxy::Create(
        TakeHandle().release(), IPC::Channel::MODE_SERVER, listener_.get(),
        thread_->task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    channel_proxy_.reset();
    thread_.reset();
    listener_.reset();
    IPCChannelMojoTestBase::TearDown();
  }

  void SendQuitMessageAndWaitForIdle() {
    sender()->Send(new WorkerMsg_Quit);
    CreateRunLoopAndRun(&listener_->run_loop_);
    EXPECT_TRUE(WaitForClientShutdown());
  }

  bool DidListenerGetBadMessage() {
    return listener_->bad_message_received_;
  }

  IPC::ChannelProxy* channel_proxy() { return channel_proxy_.get(); }
  IPC::Sender* sender() { return channel_proxy_.get(); }

 private:
  std::unique_ptr<base::Thread> thread_;
  std::unique_ptr<QuitListener> listener_;
  std::unique_ptr<IPC::ChannelProxy> channel_proxy_;
};


class IPCChannelBadMessageTest : public IPCChannelMojoTestBase {
 public:
  void SetUp() override {
    IPCChannelMojoTestBase::SetUp();

    Init("ChannelProxyClient");

    listener_ = std::make_unique<QuitListener>();
    CreateChannel(listener_.get());
    ASSERT_TRUE(ConnectChannel());
  }

  void TearDown() override {
    IPCChannelMojoTestBase::TearDown();
    listener_.reset();
  }

  void SendQuitMessageAndWaitForIdle() {
    sender()->Send(new WorkerMsg_Quit);
    CreateRunLoopAndRun(&listener_->run_loop_);
    EXPECT_TRUE(WaitForClientShutdown());
  }

  bool DidListenerGetBadMessage() {
    return listener_->bad_message_received_;
  }

 private:
  std::unique_ptr<QuitListener> listener_;
};

TEST_F(IPCChannelBadMessageTest, BadMessage) {
  sender()->Send(new TestMsg_SendBadMessage());
  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(ChannelProxyClient) {
  ChannelReflectorListener listener;
  Connect(&listener);
  listener.Init(channel());

  CreateRunLoopAndRun(&listener.run_loop_);

  Close();
}

}  // namespace
