// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/message_filter.h"

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "ipc/ipc_channel_proxy_unittest_messages.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
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
    CHECK(false);
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

class MessageCountFilter : public IPC::MessageFilter {
 public:
  enum FilterEvent {
    NONE,
    FILTER_ADDED,
    CHANNEL_CONNECTED,
    CHANNEL_ERROR,
    CHANNEL_CLOSING,
    FILTER_REMOVED
  };

  MessageCountFilter() = default;
  MessageCountFilter(uint32_t supported_message_class)
      : supported_message_class_(supported_message_class),
        is_global_filter_(false) {}

  void OnFilterAdded(IPC::Channel* channel) override {
    EXPECT_TRUE(channel);
    EXPECT_EQ(NONE, last_filter_event_);
    last_filter_event_ = FILTER_ADDED;
  }

  void OnChannelConnected(int32_t peer_pid) override {
    EXPECT_EQ(FILTER_ADDED, last_filter_event_);
    EXPECT_NE(static_cast<int32_t>(base::kNullProcessId), peer_pid);
    last_filter_event_ = CHANNEL_CONNECTED;
  }

  void OnChannelError() override {
    EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);
    last_filter_event_ = CHANNEL_ERROR;
  }

  void OnChannelClosing() override {
    // We may or may not have gotten OnChannelError; if not, the last event has
    // to be OnChannelConnected.
    EXPECT_NE(FILTER_REMOVED, last_filter_event_);
    if (last_filter_event_ != CHANNEL_ERROR)
      EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);
    last_filter_event_ = CHANNEL_CLOSING;
  }

  void OnFilterRemoved() override {
    // A filter may be removed at any time, even before the channel is connected
    // (and thus before OnFilterAdded is ever able to dispatch.) The only time
    // we won't see OnFilterRemoved is immediately after OnFilterAdded, because
    // OnChannelConnected is always the next event to fire after that.
    EXPECT_NE(FILTER_ADDED, last_filter_event_);
    last_filter_event_ = FILTER_REMOVED;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    // We should always get the OnFilterAdded and OnChannelConnected events
    // prior to any messages.
    EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);

    if (!is_global_filter_) {
      EXPECT_EQ(supported_message_class_, IPC_MESSAGE_CLASS(message));
    }
    ++messages_received_;

    if (!message_filtering_enabled_)
      return false;

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageCountFilter, message)
      IPC_MESSAGE_HANDLER(TestMsg_BadMessage, OnBadMessage)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnBadMessage(const BadType& bad_type) {
    // Should never be called since IPC wouldn't be deserialized correctly.
    CHECK(false);
  }

  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override {
    if (is_global_filter_)
      return false;
    supported_message_classes->push_back(supported_message_class_);
    return true;
  }

  void set_message_filtering_enabled(bool enabled) {
    message_filtering_enabled_ = enabled;
  }

  size_t messages_received() const { return messages_received_; }
  FilterEvent last_filter_event() const { return last_filter_event_; }

 private:
  ~MessageCountFilter() override = default;

  size_t messages_received_ = 0;
  uint32_t supported_message_class_ = 0;
  bool is_global_filter_ = true;

  FilterEvent last_filter_event_ = NONE;
  bool message_filtering_enabled_ = false;
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

TEST_F(IPCChannelProxyTest, MessageClassFilters) {
  // Construct a filter per message class.
  std::vector<scoped_refptr<MessageCountFilter>> class_filters;
  class_filters.push_back(
      base::MakeRefCounted<MessageCountFilter>(TestMsgStart));
  class_filters.push_back(
      base::MakeRefCounted<MessageCountFilter>(AutomationMsgStart));
  for (size_t i = 0; i < class_filters.size(); ++i)
    channel_proxy()->AddFilter(class_filters[i].get());

  // Send a message for each class; each filter should receive just one message.
  sender()->Send(new TestMsg_Bounce);
  sender()->Send(new AutomationMsg_Bounce);

  // Send some messages not assigned to a specific or valid message class.
  sender()->Send(new WorkerMsg_Bounce);

  // Each filter should have received just the one sent message of the
  // corresponding class.
  SendQuitMessageAndWaitForIdle();
  for (size_t i = 0; i < class_filters.size(); ++i)
    EXPECT_EQ(1U, class_filters[i]->messages_received());
}

TEST_F(IPCChannelProxyTest, GlobalAndMessageClassFilters) {
  // Add a class and global filter.
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(class_filter.get());

  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());
  global_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(global_filter.get());

  // A message  of class Test should be seen by both the global filter and
  // Test-specific filter.
  sender()->Send(new TestMsg_Bounce);

  // A message of a different class should be seen only by the global filter.
  sender()->Send(new AutomationMsg_Bounce);

  // Flush all messages.
  SendQuitMessageAndWaitForIdle();

  // The class filter should have received only the class-specific message.
  EXPECT_EQ(1U, class_filter->messages_received());

  // The global filter should have received both messages, as well as the final
  // QUIT message.
  EXPECT_EQ(3U, global_filter->messages_received());
}

TEST_F(IPCChannelProxyTest, FilterRemoval) {
  // Add a class and global filter.
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());

  // Add and remove both types of filters.
  channel_proxy()->AddFilter(class_filter.get());
  channel_proxy()->AddFilter(global_filter.get());
  channel_proxy()->RemoveFilter(global_filter.get());
  channel_proxy()->RemoveFilter(class_filter.get());

  // Send some messages; they should not be seen by either filter.
  sender()->Send(new TestMsg_Bounce);
  sender()->Send(new AutomationMsg_Bounce);

  // Ensure that the filters were removed and did not receive any messages.
  SendQuitMessageAndWaitForIdle();
  EXPECT_EQ(MessageCountFilter::FILTER_REMOVED,
            global_filter->last_filter_event());
  EXPECT_EQ(MessageCountFilter::FILTER_REMOVED,
            class_filter->last_filter_event());
  EXPECT_EQ(0U, class_filter->messages_received());
  EXPECT_EQ(0U, global_filter->messages_received());
}

TEST_F(IPCChannelProxyTest, BadMessageOnListenerThread) {
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(class_filter.get());

  sender()->Send(new TestMsg_SendBadMessage());

  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}

TEST_F(IPCChannelProxyTest, BadMessageOnIPCThread) {
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(true);
  channel_proxy()->AddFilter(class_filter.get());

  sender()->Send(new TestMsg_SendBadMessage());

  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}

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
