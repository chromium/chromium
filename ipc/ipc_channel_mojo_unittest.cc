// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_shared_memory_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo_unittest.test-mojom.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/ipc_test.mojom.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_channel_listener.h"
#include "ipc/urgent_message_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/urgent_message_scope.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace ipc_channel_mojo_unittest {
namespace {

void SendString(IPC::Sender* sender, const std::string& str) {
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  message->WriteString(str);
  ASSERT_TRUE(sender->Send(message));
}

void SendValue(IPC::Sender* sender, int32_t value) {
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(value);
  ASSERT_TRUE(sender->Send(message));
}

class ListenerThatExpectsOK : public IPC::Listener {
 public:
  explicit ListenerThatExpectsOK(base::OnceClosure quit_closure)
      : received_ok_(false), quit_closure_(std::move(quit_closure)) {}

  ~ListenerThatExpectsOK() override = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    std::string should_be_ok;
    EXPECT_TRUE(iter.ReadString(&should_be_ok));
    EXPECT_EQ(should_be_ok, "OK");
    received_ok_ = true;
    std::move(quit_closure_).Run();
    return true;
  }

  void OnChannelError() override {
    // The connection should be healthy while the listener is waiting
    // message.  An error can occur after that because the peer
    // process dies.
    CHECK(received_ok_);
  }

  static void SendOK(IPC::Sender* sender) { SendString(sender, "OK"); }

 private:
  bool received_ok_;
  base::OnceClosure quit_closure_;
};

class TestListenerBase : public IPC::Listener {
 public:
  explicit TestListenerBase(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~TestListenerBase() override = default;
  void OnChannelError() override { RunQuitClosure(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }
  IPC::Sender* sender() const { return sender_; }
  void RunQuitClosure() {
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<IPC::Sender> sender_ = nullptr;
  base::OnceClosure quit_closure_;
};

using IPCChannelMojoTest = IPCChannelMojoTestBase;

class TestChannelListenerWithExtraExpectations
    : public IPC::TestChannelListener {
 public:
  TestChannelListenerWithExtraExpectations() : is_connected_called_(false) {}

  void OnChannelConnected(int32_t peer_pid) override {
    IPC::TestChannelListener::OnChannelConnected(peer_pid);
    EXPECT_TRUE(base::kNullProcessId != peer_pid);
    is_connected_called_ = true;
  }

  bool is_connected_called() const { return is_connected_called_; }

 private:
  bool is_connected_called_;
};

TEST_F(IPCChannelMojoTest, ConnectedFromClient) {
  Init("IPCChannelMojoTestClient");

  // Set up IPC channel and start client.
  TestChannelListenerWithExtraExpectations listener;
  base::RunLoop loop;
  listener.set_quit_closure(loop.QuitWhenIdleClosure());
  CreateChannel(&listener);
  listener.Init(sender());
  ASSERT_TRUE(ConnectChannel());

  IPC::TestChannelListener::SendOneMessage(sender(), "hello from parent");
  loop.Run();

  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.is_connected_called());
  EXPECT_TRUE(listener.HasSentAll());

  DestroyChannel();
}

// A long running process that connects to us
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestClient) {
  TestChannelListenerWithExtraExpectations listener;
  Connect(&listener);
  listener.Init(channel());

  IPC::TestChannelListener::SendOneMessage(channel(), "hello from child");
  base::RunLoop loop;
  listener.set_quit_closure(loop.QuitWhenIdleClosure());
  loop.Run();
  EXPECT_TRUE(listener.is_connected_called());
  EXPECT_TRUE(listener.HasSentAll());

  Close();
}

class ListenerExpectingErrors : public TestListenerBase {
 public:
  ListenerExpectingErrors(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)), has_error_(false) {}

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelError() override {
    has_error_ = true;
    TestListenerBase::OnChannelError();
  }

  bool has_error() const { return has_error_; }

 private:
  bool has_error_;
};

class ListenerThatQuits : public IPC::Listener {
 public:
  explicit ListenerThatQuits(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelConnected(int32_t peer_pid) override {
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

// A long running process that connects to us.
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoErraticTestClient) {
  base::RunLoop run_loop;
  ListenerThatQuits listener(run_loop.QuitClosure());
  Connect(&listener);

  run_loop.Run();

  Close();
}

TEST_F(IPCChannelMojoTest, SendFailWithPendingMessages) {
  Init("IPCChannelMojoErraticTestClient");

  // Set up IPC channel and start client.
  base::RunLoop run_loop;
  ListenerExpectingErrors listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  // This matches a value in mojo/edk/system/constants.h
  const int kMaxMessageNumBytes = 4 * 1024 * 1024;
  std::string overly_large_data(kMaxMessageNumBytes, '*');
  // This messages are queued as pending.
  for (size_t i = 0; i < 10; ++i) {
    IPC::TestChannelListener::SendOneMessage(sender(),
                                             overly_large_data.c_str());
  }

  run_loop.Run();

  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.has_error());

  DestroyChannel();
}

class ListenerThatBindsATestStructPasser : public IPC::Listener,
                                           public IPC::mojom::TestStructPasser {
 public:
  ListenerThatBindsATestStructPasser() = default;
  ~ListenerThatBindsATestStructPasser() override = default;

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelConnected(int32_t peer_pid) override {}

  void OnChannelError() override { NOTREACHED(); }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    CHECK_EQ(interface_name, IPC::mojom::TestStructPasser::Name_);
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::TestStructPasser>(
            std::move(handle)));
  }

 private:
  // IPC::mojom::TestStructPasser:
  void Pass(IPC::mojom::TestStructPtr) override { NOTREACHED(); }

  mojo::AssociatedReceiver<IPC::mojom::TestStructPasser> receiver_{this};
};

class ListenerThatExpectsNoError : public IPC::Listener {
 public:
  ListenerThatExpectsNoError(base::OnceClosure connect_closure,
                             base::OnceClosure quit_closure)
      : connect_closure_(std::move(connect_closure)),
        quit_closure_(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    std::string should_be_ok;
    EXPECT_TRUE(iter.ReadString(&should_be_ok));
    EXPECT_EQ(should_be_ok, "OK");
    std::move(quit_closure_).Run();
    return true;
  }

  void OnChannelConnected(int32_t peer_pid) override {
    std::move(connect_closure_).Run();
  }

  void OnChannelError() override { NOTREACHED(); }

 private:
  base::OnceClosure connect_closure_;
  base::OnceClosure quit_closure_;
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(
    IPCChannelMojoNoImplicitChanelClosureClient) {
  base::RunLoop wait_to_connect_loop;
  base::RunLoop wait_to_quit_loop;
  ListenerThatExpectsNoError listener(wait_to_connect_loop.QuitClosure(),
                                      wait_to_quit_loop.QuitClosure());
  Connect(&listener);
  wait_to_connect_loop.Run();

  mojo::AssociatedRemote<IPC::mojom::TestStructPasser> passer;
  channel()->GetAssociatedInterfaceSupport()->GetRemoteAssociatedInterface(
      passer.BindNewEndpointAndPassReceiver());

  // This avoids hitting DCHECKs in the serialization code meant to stop us from
  // making such "mistakes" as the one we're about to make below.
  mojo::internal::SerializationWarningObserverForTesting suppress_those_dchecks;

  // Send an invalid message. The TestStruct argument is not allowed to be null.
  // This will elicit a validation error in the parent process, but should not
  // actually disconnect the channel.
  passer->Pass(nullptr);

  // Wait until the parent says it's OK to quit, so it has time to verify its
  // expected behavior.
  wait_to_quit_loop.Run();

  Close();
}

TEST_F(IPCChannelMojoTest, NoImplicitChannelClosure) {
  // Verifies that OnChannelError is not invoked due to conditions other than
  // peer closure (e.g. a malformed inbound message). Instead we should always
  // be able to handle validation errors via Mojo bad message reporting.

  // NOTE: We can't create a RunLoop before Init() is called, but we have to set
  // the default ProcessErrorCallback (which we want to reference the RunLoop)
  // before Init() launches a child process. Hence the std::optional here.
  std::optional<base::RunLoop> wait_for_error_loop;
  bool process_error_received = false;
  mojo::SetDefaultProcessErrorHandler(
      base::BindLambdaForTesting([&](const std::string&) {
        process_error_received = true;
        wait_for_error_loop->Quit();
      }));

  Init("IPCChannelMojoNoImplicitChanelClosureClient");

  wait_for_error_loop.emplace();
  ListenerThatBindsATestStructPasser listener;
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  wait_for_error_loop->Run();
  EXPECT_TRUE(process_error_received);
  mojo::SetDefaultProcessErrorHandler(base::NullCallback());

  // Tell the child it can quit and wait for it to shut down.
  ListenerThatExpectsOK::SendOK(channel());
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

struct TestingMessagePipe {
  TestingMessagePipe() {
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateMessagePipe(nullptr, &self, &peer));
  }

  mojo::ScopedMessagePipeHandle self;
  mojo::ScopedMessagePipeHandle peer;
};

class HandleSendingHelper {
 public:
  static std::string GetSendingFileContent() { return "Hello"; }

  static void WritePipe(IPC::Message* message, TestingMessagePipe* pipe) {
    std::string content = HandleSendingHelper::GetSendingFileContent();
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::WriteMessageRaw(pipe->self.get(), &content[0],
                                    static_cast<uint32_t>(content.size()),
                                    nullptr, 0, 0));
    EXPECT_TRUE(IPC::MojoMessageHelper::WriteMessagePipeTo(
        message, std::move(pipe->peer)));
  }

  static void WritePipeThenSend(IPC::Sender* sender, TestingMessagePipe* pipe) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WritePipe(message, pipe);
    ASSERT_TRUE(sender->Send(message));
  }

  static void ReadReceivedPipe(const IPC::Message& message,
                               base::PickleIterator* iter) {
    mojo::ScopedMessagePipeHandle pipe;
    EXPECT_TRUE(
        IPC::MojoMessageHelper::ReadMessagePipeFrom(&message, iter, &pipe));
    std::vector<uint8_t> content;

    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE));
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::ReadMessageRaw(pipe.get(), &content, nullptr, 0));
    EXPECT_EQ(std::string(content.begin(), content.end()),
              GetSendingFileContent());
  }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  static base::FilePath GetSendingFilePath(const base::FilePath& dir_path) {
    return dir_path.Append("ListenerThatExpectsFile.txt");
  }

  static void WriteFile(IPC::Message* message, base::File& file) {
    file.WriteAtCurrentPos(base::as_byte_span(GetSendingFileContent()));
    file.Flush();
    message->WriteAttachment(new IPC::internal::PlatformFileAttachment(
        base::ScopedFD(file.TakePlatformFile())));
  }

  static void WriteFileThenSend(IPC::Sender* sender, base::File& file) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WriteFile(message, file);
    ASSERT_TRUE(sender->Send(message));
  }

  static void WriteFileAndPipeThenSend(IPC::Sender* sender,
                                       base::File& file,
                                       TestingMessagePipe* pipe) {
    IPC::Message* message =
        new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
    WriteFile(message, file);
    WritePipe(message, pipe);
    ASSERT_TRUE(sender->Send(message));
  }

  static void ReadReceivedFile(const IPC::Message& message,
                               base::PickleIterator* iter) {
    scoped_refptr<base::Pickle::Attachment> attachment;
    EXPECT_TRUE(message.ReadAttachment(iter, &attachment));
    EXPECT_EQ(
        IPC::MessageAttachment::Type::PLATFORM_FILE,
        static_cast<IPC::MessageAttachment*>(attachment.get())->GetType());
    base::File file(
        static_cast<IPC::internal::PlatformFileAttachment*>(attachment.get())
            ->TakePlatformFile());
    std::string content(GetSendingFileContent().size(), ' ');
    file.Read(0, base::as_writable_byte_span(content));
    EXPECT_EQ(content, GetSendingFileContent());
  }
#endif
};

class ListenerThatExpectsMessagePipe : public TestListenerBase {
 public:
  ListenerThatExpectsMessagePipe(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  ~ListenerThatExpectsMessagePipe() override = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    ListenerThatExpectsOK::SendOK(sender());
    return true;
  }
};

TEST_F(IPCChannelMojoTest, SendMessagePipe) {
  Init("IPCChannelMojoTestSendMessagePipeClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  TestingMessagePipe pipe;
  HandleSendingHelper::WritePipeThenSend(channel(), &pipe);

  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestSendMessagePipeClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsMessagePipe listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}

void ReadOK(mojo::MessagePipeHandle pipe) {
  std::vector<uint8_t> should_be_ok;
  CHECK_EQ(MOJO_RESULT_OK, mojo::Wait(pipe, MOJO_HANDLE_SIGNAL_READABLE));
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::ReadMessageRaw(pipe, &should_be_ok, nullptr, 0));
  EXPECT_EQ("OK", std::string(should_be_ok.begin(), should_be_ok.end()));
}

void WriteOK(mojo::MessagePipeHandle pipe) {
  std::string ok("OK");
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::WriteMessageRaw(pipe, &ok[0], static_cast<uint32_t>(ok.size()),
                                 nullptr, 0, 0));
}

class ListenerThatExpectsMessagePipeUsingParamTrait : public TestListenerBase {
 public:
  explicit ListenerThatExpectsMessagePipeUsingParamTrait(
      base::OnceClosure quit_closure,
      bool receiving_valid)
      : TestListenerBase(std::move(quit_closure)),
        receiving_valid_(receiving_valid) {}

  ~ListenerThatExpectsMessagePipeUsingParamTrait() override = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    mojo::MessagePipeHandle handle;
    EXPECT_TRUE(IPC::ParamTraits<mojo::MessagePipeHandle>::Read(&message, &iter,
                                                                &handle));
    EXPECT_EQ(handle.is_valid(), receiving_valid_);
    if (receiving_valid_) {
      ReadOK(handle);
      MojoClose(handle.value());
    }

    ListenerThatExpectsOK::SendOK(sender());
    return true;
  }

 private:
  bool receiving_valid_;
};

class ParamTraitMessagePipeClient : public IpcChannelMojoTestClient {
 public:
  void RunTest(bool receiving_valid_handle) {
    base::RunLoop run_loop;
    ListenerThatExpectsMessagePipeUsingParamTrait listener(
        run_loop.QuitClosure(), receiving_valid_handle);
    Connect(&listener);
    listener.set_sender(channel());

    run_loop.Run();

    Close();
  }
};

TEST_F(IPCChannelMojoTest, ParamTraitValidMessagePipe) {
  Init("ParamTraitValidMessagePipeClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  TestingMessagePipe pipe;

  std::unique_ptr<IPC::Message> message(new IPC::Message());
  IPC::ParamTraits<mojo::MessagePipeHandle>::Write(message.get(),
                                                   pipe.peer.release());
  WriteOK(pipe.self.get());

  channel()->Send(message.release());
  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    ParamTraitValidMessagePipeClient,
    ParamTraitMessagePipeClient) {
  RunTest(true);
}

TEST_F(IPCChannelMojoTest, ParamTraitInvalidMessagePipe) {
  Init("ParamTraitInvalidMessagePipeClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  mojo::MessagePipeHandle invalid_handle;
  std::unique_ptr<IPC::Message> message(new IPC::Message());
  IPC::ParamTraits<mojo::MessagePipeHandle>::Write(message.get(),
                                                   invalid_handle);

  channel()->Send(message.release());
  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    ParamTraitInvalidMessagePipeClient,
    ParamTraitMessagePipeClient) {
  RunTest(false);
}

TEST_F(IPCChannelMojoTest, SendFailAfterClose) {
  Init("IPCChannelMojoTestSendOkClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  run_loop.Run();
  channel()->Close();
  ASSERT_FALSE(channel()->Send(new IPC::Message()));

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

class ListenerSendingOneOk : public TestListenerBase {
 public:
  ListenerSendingOneOk(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelConnected(int32_t peer_pid) override {
    ListenerThatExpectsOK::SendOK(sender());
    RunQuitClosure();
  }
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestSendOkClient) {
  base::RunLoop run_loop;
  ListenerSendingOneOk listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}

class ChannelProxyRunner {
 public:
  ChannelProxyRunner(mojo::ScopedMessagePipeHandle handle,
                     bool for_server)
      : for_server_(for_server),
        handle_(std::move(handle)),
        io_thread_("ChannelProxyRunner IO thread"),
        never_signaled_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {
  }

  ChannelProxyRunner(const ChannelProxyRunner&) = delete;
  ChannelProxyRunner& operator=(const ChannelProxyRunner&) = delete;

  void CreateProxy(
      IPC::Listener* listener,
      IPC::UrgentMessageObserver* urgent_message_observer = nullptr) {
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    proxy_ = IPC::SyncChannel::Create(
        listener, io_thread_.task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &never_signaled_);
    proxy_->SetUrgentMessageObserver(urgent_message_observer);
  }

  void RunProxy() {
    std::unique_ptr<IPC::ChannelFactory> factory;
    if (for_server_) {
      factory = IPC::ChannelMojo::CreateServerFactory(
          std::move(handle_), io_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
    } else {
      factory = IPC::ChannelMojo::CreateClientFactory(
          std::move(handle_), io_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
    }
    proxy_->Init(std::move(factory), true);
  }

  IPC::ChannelProxy* proxy() { return proxy_.get(); }

 private:
  const bool for_server_;

  mojo::ScopedMessagePipeHandle handle_;
  base::Thread io_thread_;
  base::WaitableEvent never_signaled_;
  std::unique_ptr<IPC::ChannelProxy> proxy_;
};

class IPCChannelProxyMojoTest : public IPCChannelMojoTestBase {
 public:
  void Init(const std::string& client_name) {
    IPCChannelMojoTestBase::Init(client_name);
    runner_ = std::make_unique<ChannelProxyRunner>(TakeHandle(), true);
  }

  void CreateProxy(
      IPC::Listener* listener,
      IPC::UrgentMessageObserver* urgent_message_observer = nullptr) {
    runner_->CreateProxy(listener, urgent_message_observer);
  }

  void RunProxy() {
    runner_->RunProxy();
  }

  void DestroyProxy() {
    runner_.reset();
    base::RunLoop().RunUntilIdle();
  }

  IPC::ChannelProxy* proxy() { return runner_->proxy(); }

 private:
  std::unique_ptr<ChannelProxyRunner> runner_;
};

class ListenerWithSimpleProxyAssociatedInterface
    : public IPC::Listener,
      public IPC::mojom::SimpleTestDriver {
 public:
  static const int kNumMessages;

  explicit ListenerWithSimpleProxyAssociatedInterface(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~ListenerWithSimpleProxyAssociatedInterface() override = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    int32_t should_be_expected;
    EXPECT_TRUE(iter.ReadInt(&should_be_expected));
    EXPECT_EQ(should_be_expected, next_expected_value_);
    num_messages_received_++;
    return true;
  }

  void OnChannelError() override { CHECK(!quit_closure_); }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    DCHECK_EQ(interface_name, IPC::mojom::SimpleTestDriver::Name_);
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestDriver>(
            std::move(handle)));
  }

  bool received_all_messages() const {
    return num_messages_received_ == kNumMessages && !quit_closure_;
  }

 private:
  // IPC::mojom::SimpleTestDriver:
  void ExpectValue(int32_t value) override {
    next_expected_value_ = value;
  }

  void GetExpectedValue(GetExpectedValueCallback callback) override {
    std::move(callback).Run(next_expected_value_);
  }

  void RequestValue(RequestValueCallback callback) override { NOTREACHED(); }

  void RequestQuit(RequestQuitCallback callback) override {
    std::move(callback).Run();
    receiver_.reset();
    std::move(quit_closure_).Run();
  }

  void BindReceiver(
      mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestDriver> receiver) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

  int32_t next_expected_value_ = 0;
  int num_messages_received_ = 0;
  base::OnceClosure quit_closure_;

  mojo::AssociatedReceiver<IPC::mojom::SimpleTestDriver> receiver_{this};
};

const int ListenerWithSimpleProxyAssociatedInterface::kNumMessages = 1000;

TEST_F(IPCChannelProxyMojoTest, ProxyThreadAssociatedInterface) {
  Init("ProxyThreadAssociatedInterfaceClient");

  base::RunLoop run_loop;
  ListenerWithSimpleProxyAssociatedInterface listener(run_loop.QuitClosure());
  CreateProxy(&listener);
  RunProxy();

  run_loop.Run();

  EXPECT_TRUE(WaitForClientShutdown());
  EXPECT_TRUE(listener.received_all_messages());

  DestroyProxy();
}

class ChannelProxyClient {
 public:
  void Init(mojo::ScopedMessagePipeHandle handle) {
    runner_ = std::make_unique<ChannelProxyRunner>(std::move(handle), false);
  }

  void CreateProxy(IPC::Listener* listener) { runner_->CreateProxy(listener); }

  void RunProxy() { runner_->RunProxy(); }

  void DestroyProxy() {
    runner_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void RequestQuitAndWaitForAck(IPC::mojom::SimpleTestDriver* driver) {
    base::RunLoop loop;
    driver->RequestQuit(loop.QuitClosure());
    loop.Run();
  }

  IPC::ChannelProxy* proxy() { return runner_->proxy(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ChannelProxyRunner> runner_;
};

class DummyListener : public IPC::Listener {
 public:
  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    ProxyThreadAssociatedInterfaceClient,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  // Send a bunch of interleaved messages, alternating between the associated
  // interface and a legacy IPC::Message.
  mojo::AssociatedRemote<IPC::mojom::SimpleTestDriver> driver;
  proxy()->GetRemoteAssociatedInterface(
      driver.BindNewEndpointAndPassReceiver());
  for (int i = 0; i < ListenerWithSimpleProxyAssociatedInterface::kNumMessages;
       ++i) {
    driver->ExpectValue(i);
    SendValue(proxy(), i);
  }
  base::RunLoop run_loop;
  driver->RequestQuit(run_loop.QuitClosure());
  run_loop.Run();

  DestroyProxy();
}

class ListenerWithIndirectProxyAssociatedInterface
    : public IPC::Listener,
      public IPC::mojom::IndirectTestDriver,
      public IPC::mojom::PingReceiver {
 public:
  ListenerWithIndirectProxyAssociatedInterface() = default;
  ~ListenerWithIndirectProxyAssociatedInterface() override = default;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    DCHECK(!driver_receiver_.is_bound());
    DCHECK_EQ(interface_name, IPC::mojom::IndirectTestDriver::Name_);
    driver_receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::IndirectTestDriver>(
            std::move(handle)));
  }

  void set_ping_handler(const base::RepeatingClosure& handler) {
    ping_handler_ = handler;
  }

 private:
  // IPC::mojom::IndirectTestDriver:
  void GetPingReceiver(mojo::PendingAssociatedReceiver<IPC::mojom::PingReceiver>
                           receiver) override {
    ping_receiver_receiver_.Bind(std::move(receiver));
  }

  // IPC::mojom::PingReceiver:
  void Ping(PingCallback callback) override {
    std::move(callback).Run();
    ping_handler_.Run();
  }

  mojo::AssociatedReceiver<IPC::mojom::IndirectTestDriver> driver_receiver_{
      this};
  mojo::AssociatedReceiver<IPC::mojom::PingReceiver> ping_receiver_receiver_{
      this};

  base::RepeatingClosure ping_handler_;
};

TEST_F(IPCChannelProxyMojoTest, ProxyThreadAssociatedInterfaceIndirect) {
  // Tests that we can pipeline interface requests and subsequent messages
  // targeting proxy thread bindings, and the channel will still dispatch
  // messages appropriately.

  Init("ProxyThreadAssociatedInterfaceIndirectClient");

  ListenerWithIndirectProxyAssociatedInterface listener;
  CreateProxy(&listener);
  RunProxy();

  base::RunLoop loop;
  listener.set_ping_handler(loop.QuitClosure());
  loop.Run();

  EXPECT_TRUE(WaitForClientShutdown());

  DestroyProxy();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    ProxyThreadAssociatedInterfaceIndirectClient,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  // Use an interface requested via another interface. On the remote end both
  // interfaces are bound on the proxy thread. This ensures that the Ping
  // message we send will still be dispatched properly even though the remote
  // endpoint may not have been bound yet by the time the message is initially
  // processed on the IO thread.
  mojo::AssociatedRemote<IPC::mojom::IndirectTestDriver> driver;
  mojo::AssociatedRemote<IPC::mojom::PingReceiver> ping_receiver;
  proxy()->GetRemoteAssociatedInterface(
      driver.BindNewEndpointAndPassReceiver());
  driver->GetPingReceiver(ping_receiver.BindNewEndpointAndPassReceiver());

  base::RunLoop loop;
  ping_receiver->Ping(loop.QuitClosure());
  loop.Run();

  DestroyProxy();
}

class ListenerWithSyncAssociatedInterface
    : public IPC::Listener,
      public IPC::mojom::SimpleTestDriver {
 public:
  ListenerWithSyncAssociatedInterface() = default;
  ~ListenerWithSyncAssociatedInterface() override = default;

  void set_sync_sender(IPC::Sender* sync_sender) { sync_sender_ = sync_sender; }

  void RunUntilQuitRequested() {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

  void CloseBinding() { receiver_.reset(); }

  void set_response_value(int32_t response) {
    response_value_ = response;
  }

 private:
  // IPC::mojom::SimpleTestDriver:
  void ExpectValue(int32_t value) override {
    next_expected_value_ = value;
  }

  void GetExpectedValue(GetExpectedValueCallback callback) override {
    std::move(callback).Run(next_expected_value_);
  }

  void RequestValue(RequestValueCallback callback) override {
    std::move(callback).Run(response_value_);
  }

  void RequestQuit(RequestQuitCallback callback) override {
    std::move(quit_closure_).Run();
    std::move(callback).Run();
  }

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override {
    EXPECT_EQ(0u, message.type());
    EXPECT_TRUE(message.is_sync());
    EXPECT_TRUE(message.should_unblock());
    std::unique_ptr<IPC::Message> reply(
        IPC::SyncMessage::GenerateReply(&message));
    reply->WriteInt(response_value_);
    DCHECK(sync_sender_);
    EXPECT_TRUE(sync_sender_->Send(reply.release()));
    return true;
  }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    DCHECK(!receiver_.is_bound());
    DCHECK_EQ(interface_name, IPC::mojom::SimpleTestDriver::Name_);
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestDriver>(
            std::move(handle)));
  }

  void BindReceiver(
      mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestDriver> receiver) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

  raw_ptr<IPC::Sender, DanglingUntriaged> sync_sender_ = nullptr;
  int32_t next_expected_value_ = 0;
  int32_t response_value_ = 0;
  base::OnceClosure quit_closure_;

  mojo::AssociatedReceiver<IPC::mojom::SimpleTestDriver> receiver_{this};
};

class SyncReplyReader : public IPC::MessageReplyDeserializer {
 public:
  explicit SyncReplyReader(int32_t* storage) : storage_(storage) {}

  SyncReplyReader(const SyncReplyReader&) = delete;
  SyncReplyReader& operator=(const SyncReplyReader&) = delete;

  ~SyncReplyReader() override = default;

 private:
  // IPC::MessageReplyDeserializer:
  bool SerializeOutputParameters(const IPC::Message& message,
                                 base::PickleIterator iter) override {
    if (!iter.ReadInt(storage_))
      return false;
    return true;
  }

  raw_ptr<int32_t> storage_;
};

TEST_F(IPCChannelProxyMojoTest, SyncAssociatedInterface) {
  Init("SyncAssociatedInterface");

  ListenerWithSyncAssociatedInterface listener;
  CreateProxy(&listener);
  listener.set_sync_sender(proxy());
  RunProxy();

  // Run the client's simple sanity check to completion.
  listener.RunUntilQuitRequested();

  // Verify that we can send a sync IPC and service an incoming sync request
  // while waiting on it
  listener.set_response_value(42);
  mojo::AssociatedRemote<IPC::mojom::SimpleTestClient> client;
  proxy()->GetRemoteAssociatedInterface(
      client.BindNewEndpointAndPassReceiver());
  int32_t received_value;
  EXPECT_TRUE(client->RequestValue(&received_value));
  EXPECT_EQ(42, received_value);

  // Do it again. This time the client will send a classical sync IPC to us
  // while we wait.
  received_value = 0;
  EXPECT_TRUE(client->RequestValue(&received_value));
  EXPECT_EQ(42, received_value);

  // Now make a classical sync IPC request to the client. It will send a
  // sync associated interface message to us while we wait.
  received_value = 0;
  auto request = std::make_unique<IPC::SyncMessage>(
      0, 0, IPC::Message::PRIORITY_NORMAL,
      std::make_unique<SyncReplyReader>(&received_value));
  EXPECT_TRUE(proxy()->Send(request.release()));
  EXPECT_EQ(42, received_value);

  listener.CloseBinding();
  EXPECT_TRUE(WaitForClientShutdown());

  DestroyProxy();
}

class SimpleTestClientImpl : public IPC::mojom::SimpleTestClient,
                             public IPC::Listener {
 public:
  SimpleTestClientImpl() = default;

  SimpleTestClientImpl(const SimpleTestClientImpl&) = delete;
  SimpleTestClientImpl& operator=(const SimpleTestClientImpl&) = delete;

  ~SimpleTestClientImpl() override = default;

  void set_driver(IPC::mojom::SimpleTestDriver* driver) { driver_ = driver; }
  void set_sync_sender(IPC::Sender* sync_sender) { sync_sender_ = sync_sender; }

  void WaitForValueRequest() { Run(); }

  void Run() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void UseSyncSenderForRequest(bool use_sync_sender) {
    use_sync_sender_ = use_sync_sender;
  }

 private:
  // IPC::mojom::SimpleTestClient:
  void RequestValue(RequestValueCallback callback) override {
    int32_t response = 0;
    if (use_sync_sender_) {
      auto reply = std::make_unique<IPC::SyncMessage>(
          0, 0, IPC::Message::PRIORITY_NORMAL,
          std::make_unique<SyncReplyReader>(&response));
      EXPECT_TRUE(sync_sender_->Send(reply.release()));
    } else {
      DCHECK(driver_);
      EXPECT_TRUE(driver_->RequestValue(&response));
    }

    std::move(callback).Run(response);

    DCHECK(run_loop_);
    run_loop_->Quit();
  }

  // No implementation needed. Only called on an endpoint which never binds its
  // receiver.
  void BindSync(
      mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestClient> receiver,
      BindSyncCallback callback) override {
    NOTREACHED();
  }

  void GetReceiverWithQueuedSyncMessage(
      GetReceiverWithQueuedSyncMessageCallback callback) override {
    // Immediately send back a sync IPC over the new pipe and expect the call to
    // be interrupted without a reply. Note that we also reply *before* issuing
    // the sync call to allow the main test process to make progress.
    mojo::AssociatedRemote<IPC::mojom::SimpleTestClient> remote;
    mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestClient>
        queued_receiver;
    {
      // The nested receiver we send will already know its peer is closed when
      // it arrives.
      mojo::AssociatedRemote<IPC::mojom::SimpleTestClient> unused;
      queued_receiver = unused.BindNewEndpointAndPassReceiver();
    }
    std::move(callback).Run(remote.BindNewEndpointAndPassReceiver());
    EXPECT_FALSE(remote->BindSync(std::move(queued_receiver)));
  }

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override {
    int32_t response;
    DCHECK(driver_);
    EXPECT_TRUE(driver_->RequestValue(&response));
    std::unique_ptr<IPC::Message> reply(
        IPC::SyncMessage::GenerateReply(&message));
    reply->WriteInt(response);
    EXPECT_TRUE(sync_sender_->Send(reply.release()));

    DCHECK(run_loop_);
    run_loop_->Quit();
    return true;
  }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    DCHECK(!receiver_.is_bound());
    DCHECK_EQ(interface_name, IPC::mojom::SimpleTestClient::Name_);

    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestClient>(
            std::move(handle)));
  }

  bool use_sync_sender_ = false;
  mojo::AssociatedReceiver<IPC::mojom::SimpleTestClient> receiver_{this};
  raw_ptr<IPC::Sender> sync_sender_ = nullptr;
  raw_ptr<IPC::mojom::SimpleTestDriver> driver_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(SyncAssociatedInterface,
                                                        ChannelProxyClient) {
  SimpleTestClientImpl client_impl;
  CreateProxy(&client_impl);
  client_impl.set_sync_sender(proxy());
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::SimpleTestDriver> driver;
  proxy()->GetRemoteAssociatedInterface(
      driver.BindNewEndpointAndPassReceiver());
  client_impl.set_driver(driver.get());

  // Simple sync message sanity check.
  driver->ExpectValue(42);
  int32_t expected_value = 0;
  EXPECT_TRUE(driver->GetExpectedValue(&expected_value));
  EXPECT_EQ(42, expected_value);
  RequestQuitAndWaitForAck(driver.get());

  // Wait for the test driver to perform a sync call test with our own sync
  // associated interface message nested inside.
  client_impl.UseSyncSenderForRequest(false);
  client_impl.WaitForValueRequest();

  // Wait for the test driver to perform a sync call test with our own classical
  // sync IPC nested inside.
  client_impl.UseSyncSenderForRequest(true);
  client_impl.WaitForValueRequest();

  // Wait for the test driver to perform a classical sync IPC request, with our
  // own sync associated interface message nested inside.
  client_impl.UseSyncSenderForRequest(false);
  client_impl.WaitForValueRequest();

  DestroyProxy();
}

class ListenerWithClumsyBinder : public IPC::Listener {
 public:
  ListenerWithClumsyBinder() = default;
  ~ListenerWithClumsyBinder() override = default;

  void RunUntilClientQuit() { run_loop_.Run(); }

 private:
  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override {
    run_loop_.Quit();
    return true;
  }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    // Ignore and drop the endpoint so it's closed.
  }

  base::RunLoop run_loop_;
};

TEST_F(IPCChannelProxyMojoTest, DropAssociatedReceiverWithSyncCallInFlight) {
  // Regression test for https://crbug.com/331636067. Verifies that endpoint
  // lifetime is properly managed when associated endpoints are serialized into
  // a message that gets dropped before transmission.

  Init("SyncCallToDroppedReceiver");
  ListenerWithClumsyBinder listener;
  CreateProxy(&listener);
  RunProxy();
  listener.RunUntilClientQuit();
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyProxy();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    SyncCallToDroppedReceiver,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  mojo::AssociatedRemote<mojom::Binder> binder;
  proxy()->GetRemoteAssociatedInterface(
      binder.BindNewEndpointAndPassReceiver());

  // Wait for disconnection to be observed. This way we know any subsequent
  // outgoing messages on `binder` will not be sent.
  base::RunLoop loop;
  binder.set_disconnect_handler(loop.QuitClosure());
  loop.Run();

  // Send another endpoint over. This receiver will be dropped, and the remote
  // should be properly notified of peer closure to terminate the sync call. The
  // call should return false (because no reply), but shouldn't hang.
  mojo::AssociatedRemote<mojom::Binder> another_binder;
  binder->Bind(another_binder.BindNewEndpointAndPassReceiver());
  EXPECT_FALSE(another_binder->Ping());

  SendString(proxy(), "ok bye");
  DestroyProxy();
}

// TODO(crbug.com/40940810): Disabled for flaky behavior of forced
// process termination. Will be re-enabled with a fix.
TEST_F(IPCChannelProxyMojoTest, DISABLED_SyncAssociatedInterfacePipeError) {
  // Regression test for https://crbug.com/1494461.

  Init("SyncAssociatedInterfacePipeError");

  ListenerWithSyncAssociatedInterface listener;
  CreateProxy(&listener);
  listener.set_sync_sender(proxy());
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::SimpleTestClient> client;
  proxy()->GetRemoteAssociatedInterface(
      client.BindNewEndpointAndPassReceiver());

  mojo::AssociatedRemote<IPC::mojom::Terminator> terminator;
  proxy()->GetRemoteAssociatedInterface(
      terminator.BindNewEndpointAndPassReceiver());

  // The setup here is to have the client process add a new associated endpoint
  // with a sync message queued on it, towards us. As soon as we receive the
  // endpoint we close it, but its state (including its inbound sync message
  // queue) isn't actually destroyed until the peer is closed too.
  //
  // Note that the client creates the endpoint rather than us, because client
  // endpoints are assigned lower interface IDs and will thus elicit the
  // necessary endpoint ordering to trigger https://crbug.com/1494461 below.
  {
    base::RunLoop loop;
    client->GetReceiverWithQueuedSyncMessage(base::BindLambdaForTesting(
        [&loop](mojo::PendingAssociatedReceiver<IPC::mojom::SimpleTestClient>
                    receiver) { loop.Quit(); }));
    loop.Run();
  }

  // If https://crbug.com/1494461 is present, it should be hit within this call,
  // as soon as client termination signals a local pipe error and marks the
  // above endpoint's peer as closed.
  EXPECT_FALSE(terminator->Terminate());

#if BUILDFLAG(IS_ANDROID)
  // NOTE: On Android, the client's forced termination will look like an error,
  // but it is not.
  WaitForClientShutdown();
#else
  EXPECT_TRUE(WaitForClientShutdown());
#endif

  DestroyProxy();
}

class TerminatorImpl : public IPC::mojom::Terminator {
 public:
  TerminatorImpl() = default;
  ~TerminatorImpl() override = default;

  static void Create(
      mojo::PendingAssociatedReceiver<IPC::mojom::Terminator> receiver) {
    mojo::MakeSelfOwnedAssociatedReceiver(std::make_unique<TerminatorImpl>(),
                                          std::move(receiver));
  }

  // IPC::mojom::Terminator:
  void Terminate(TerminateCallback callback) override {
    base::Process::TerminateCurrentProcessImmediately(0);
  }
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    SyncAssociatedInterfacePipeError,
    ChannelProxyClient) {
  SimpleTestClientImpl client_impl;
  CreateProxy(&client_impl);

  // Let the IO thread receive a message to self-terminate this process. This
  // is used to forcibly shut down the client on the test's request. Without
  // doing this (and doing a clean shutdown instead) the client will clean up
  // interfaces and make it impossible to trigger the regression path for
  // https://crbug.com/1494461.
  proxy()->AddAssociatedInterfaceForIOThread(
      base::BindRepeating(&TerminatorImpl::Create));

  RunProxy();
  client_impl.Run();
  DestroyProxy();
}

TEST_F(IPCChannelProxyMojoTest, Pause) {
  // Ensures that pausing a channel elicits the expected behavior when sending
  // messages, unpausing, sending more messages, and then manually flushing.
  // Specifically a sequence like:
  //
  //   Connect()
  //   Send(A)
  //   Pause()
  //   Send(B)
  //   Send(C)
  //   Unpause(false)
  //   Send(D)
  //   Send(E)
  //   Flush()
  //
  // must result in the other end receiving messages A, D, E, B, D; in that
  // order.
  //
  // This behavior is required by some consumers of IPC::Channel, and it is not
  // sufficient to leave this up to the consumer to implement since associated
  // interface requests and messages also need to be queued according to the
  // same policy.
  Init("CreatePausedClient");

  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  // This message must be sent immediately since the channel is unpaused.
  SendValue(proxy(), 1);

  proxy()->Pause();

  // These messages must be queued internally since the channel is paused.
  SendValue(proxy(), 2);
  SendValue(proxy(), 3);

  proxy()->Unpause(false /* flush */);

  // These messages must be sent immediately since the channel is unpaused.
  SendValue(proxy(), 4);
  SendValue(proxy(), 5);

  // Now we flush the previously queued messages.
  proxy()->Flush();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyProxy();
}

class ExpectValueSequenceListener : public IPC::Listener {
 public:
  ExpectValueSequenceListener(base::queue<int32_t>* expected_values,
                              base::OnceClosure quit_closure)
      : expected_values_(expected_values),
        quit_closure_(std::move(quit_closure)) {}

  ExpectValueSequenceListener(const ExpectValueSequenceListener&) = delete;
  ExpectValueSequenceListener& operator=(const ExpectValueSequenceListener&) =
      delete;

  ~ExpectValueSequenceListener() override = default;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK(!expected_values_->empty());
    base::PickleIterator iter(message);
    int32_t should_be_expected;
    EXPECT_TRUE(iter.ReadInt(&should_be_expected));
    EXPECT_EQ(expected_values_->front(), should_be_expected);
    expected_values_->pop();
    if (expected_values_->empty())
      std::move(quit_closure_).Run();
    return true;
  }

 private:
  raw_ptr<base::queue<int32_t>> expected_values_;
  base::OnceClosure quit_closure_;
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(CreatePausedClient,
                                                        ChannelProxyClient) {
  base::queue<int32_t> expected_values;
  base::RunLoop run_loop;
  ExpectValueSequenceListener listener(&expected_values,
                                       run_loop.QuitClosure());
  CreateProxy(&listener);
  expected_values.push(1);
  expected_values.push(4);
  expected_values.push(5);
  expected_values.push(2);
  expected_values.push(3);
  RunProxy();
  run_loop.Run();
  EXPECT_TRUE(expected_values.empty());
  DestroyProxy();
}

TEST_F(IPCChannelProxyMojoTest, AssociatedRequestClose) {
  Init("DropAssociatedRequest");

  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::AssociatedInterfaceVendor> vendor;
  proxy()->GetRemoteAssociatedInterface(
      vendor.BindNewEndpointAndPassReceiver());
  mojo::AssociatedRemote<IPC::mojom::SimpleTestDriver> tester;
  vendor->GetTestInterface(tester.BindNewEndpointAndPassReceiver());
  base::RunLoop run_loop;
  tester.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  tester.reset();
  proxy()->GetRemoteAssociatedInterface(
      tester.BindNewEndpointAndPassReceiver());
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyProxy();
}

class AssociatedInterfaceDroppingListener : public IPC::Listener {
 public:
  AssociatedInterfaceDroppingListener(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    if (interface_name == IPC::mojom::SimpleTestDriver::Name_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(DropAssociatedRequest,
                                                        ChannelProxyClient) {
  base::RunLoop run_loop;
  AssociatedInterfaceDroppingListener listener(run_loop.QuitClosure());
  CreateProxy(&listener);
  RunProxy();
  run_loop.Run();
  DestroyProxy();
}

#if !BUILDFLAG(IS_APPLE)
// TODO(wez): On Mac we need to set up a MachPortBroker before we can transfer
// Mach ports (which underpin Sharedmemory on Mac) across IPC.

template <class SharedMemoryRegionType>
class IPCChannelMojoSharedMemoryRegionTypedTest : public IPCChannelMojoTest {};

struct WritableRegionTraits {
  using RegionType = base::WritableSharedMemoryRegion;
  static const char kClientName[];
};
const char WritableRegionTraits::kClientName[] =
    "IPCChannelMojoTestSendWritableSharedMemoryRegionClient";
struct UnsafeRegionTraits {
  using RegionType = base::UnsafeSharedMemoryRegion;
  static const char kClientName[];
};
const char UnsafeRegionTraits::kClientName[] =
    "IPCChannelMojoTestSendUnsafeSharedMemoryRegionClient";
struct ReadOnlyRegionTraits {
  using RegionType = base::ReadOnlySharedMemoryRegion;
  static const char kClientName[];
};
const char ReadOnlyRegionTraits::kClientName[] =
    "IPCChannelMojoTestSendReadOnlySharedMemoryRegionClient";

typedef ::testing::
    Types<WritableRegionTraits, UnsafeRegionTraits, ReadOnlyRegionTraits>
        AllSharedMemoryRegionTraits;
TYPED_TEST_SUITE(IPCChannelMojoSharedMemoryRegionTypedTest,
                 AllSharedMemoryRegionTraits);

template <class SharedMemoryRegionType>
class ListenerThatExpectsSharedMemoryRegion : public TestListenerBase {
 public:
  explicit ListenerThatExpectsSharedMemoryRegion(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);

    SharedMemoryRegionType region;
    EXPECT_TRUE(IPC::ReadParam(&message, &iter, &region));
    EXPECT_TRUE(region.IsValid());

    // Verify the shared memory region has expected content.
    typename SharedMemoryRegionType::MappingType mapping = region.Map();
    std::string content = HandleSendingHelper::GetSendingFileContent();
    EXPECT_EQ(0, memcmp(mapping.memory(), content.data(), content.size()));

    ListenerThatExpectsOK::SendOK(sender());
    return true;
  }
};

TYPED_TEST(IPCChannelMojoSharedMemoryRegionTypedTest, Send) {
  this->Init(TypeParam::kClientName);

  const size_t size = 1004;
  typename TypeParam::RegionType region;
  base::WritableSharedMemoryMapping mapping;
  std::tie(region, mapping) =
      base::CreateMappedRegion<typename TypeParam::RegionType>(size);

  std::string content = HandleSendingHelper::GetSendingFileContent();
  memcpy(mapping.memory(), content.data(), content.size());

  // Create a success listener, and launch the child process.
  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  this->CreateChannel(&listener);
  ASSERT_TRUE(this->ConnectChannel());

  // Send the child process an IPC with |shmem| attached, to verify
  // that is is correctly wrapped, transferred and unwrapped.
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(message, region);
  ASSERT_TRUE(this->channel()->Send(message));

  run_loop.Run();

  this->channel()->Close();

  EXPECT_TRUE(this->WaitForClientShutdown());
  EXPECT_FALSE(region.IsValid());
  this->DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(
    IPCChannelMojoTestSendWritableSharedMemoryRegionClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsSharedMemoryRegion<base::WritableSharedMemoryRegion>
      listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(
    IPCChannelMojoTestSendUnsafeSharedMemoryRegionClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsSharedMemoryRegion<base::UnsafeSharedMemoryRegion>
      listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(
    IPCChannelMojoTestSendReadOnlySharedMemoryRegionClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsSharedMemoryRegion<base::ReadOnlySharedMemoryRegion>
      listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}
#endif  // !BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

class ListenerThatExpectsFile : public TestListenerBase {
 public:
  explicit ListenerThatExpectsFile(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    ListenerThatExpectsOK::SendOK(sender());
    return true;
  }
};

TEST_F(IPCChannelMojoTest, SendPlatformFile) {
  Init("IPCChannelMojoTestSendPlatformFileClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::File file(HandleSendingHelper::GetSendingFilePath(temp_dir.GetPath()),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                      base::File::FLAG_READ);
  HandleSendingHelper::WriteFileThenSend(channel(), file);
  run_loop.Run();

  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestSendPlatformFileClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsFile listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}

class ListenerThatExpectsFileAndMessagePipe : public TestListenerBase {
 public:
  explicit ListenerThatExpectsFileAndMessagePipe(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  ~ListenerThatExpectsFileAndMessagePipe() override = default;

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    HandleSendingHelper::ReadReceivedFile(message, &iter);
    HandleSendingHelper::ReadReceivedPipe(message, &iter);
    ListenerThatExpectsOK::SendOK(sender());
    return true;
  }
};

TEST_F(IPCChannelMojoTest, SendPlatformFileAndMessagePipe) {
  Init("IPCChannelMojoTestSendPlatformFileAndMessagePipeClient");

  base::RunLoop run_loop;
  ListenerThatExpectsOK listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::File file(HandleSendingHelper::GetSendingFilePath(temp_dir.GetPath()),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                      base::File::FLAG_READ);
  TestingMessagePipe pipe;
  HandleSendingHelper::WriteFileAndPipeThenSend(channel(), file, &pipe);

  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(
    IPCChannelMojoTestSendPlatformFileAndMessagePipeClient) {
  base::RunLoop run_loop;
  ListenerThatExpectsFileAndMessagePipe listener(run_loop.QuitClosure());
  Connect(&listener);
  listener.set_sender(channel());

  run_loop.Run();

  Close();
}

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

const base::ProcessId kMagicChildId = 54321;

class ListenerThatVerifiesPeerPid : public TestListenerBase {
 public:
  explicit ListenerThatVerifiesPeerPid(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  void OnChannelConnected(int32_t peer_pid) override {
    EXPECT_EQ(peer_pid, kMagicChildId);
    RunQuitClosure();
  }

  bool OnMessageReceived(const IPC::Message& message) override { NOTREACHED(); }
};

// The global PID is only used on systems that use the zygote. Hence, this
// test is disabled on other platforms.
TEST_F(IPCChannelMojoTest, VerifyGlobalPid) {
  Init("IPCChannelMojoTestVerifyGlobalPidClient");

  base::RunLoop run_loop;
  ListenerThatVerifiesPeerPid listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestVerifyGlobalPidClient) {
  IPC::Channel::SetGlobalPid(kMagicChildId);

  base::RunLoop run_loop;
  ListenerThatQuits listener(run_loop.QuitClosure());
  Connect(&listener);

  run_loop.Run();

  Close();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

class ListenerWithUrgentMessageAssociatedInterface
    : public IPC::mojom::InterfaceWithUrgentMethod,
      public IPC::Listener,
      public IPC::UrgentMessageObserver {
 public:
  explicit ListenerWithUrgentMessageAssociatedInterface(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ListenerWithUrgentMessageAssociatedInterface(
      const ListenerWithUrgentMessageAssociatedInterface&) = delete;
  ListenerWithUrgentMessageAssociatedInterface& operator=(
      const ListenerWithUrgentMessageAssociatedInterface&) = delete;

  ~ListenerWithUrgentMessageAssociatedInterface() override = default;

  uint32_t num_maybe_urgent_messages_received() const {
    return num_maybe_urgent_messages_received_;
  }

  uint32_t num_urgent_messages_received() const {
    return num_urgent_messages_received_;
  }

  uint32_t num_non_urgent_messages_received() const {
    return num_non_urgent_messages_received_;
  }

  uint32_t num_observer_urgent_messages_received() const {
    return num_observer_urgent_messages_received_.load(
        std::memory_order_relaxed);
  }

  uint32_t num_observer_urgent_messages_processed() const {
    return num_observer_urgent_messages_processed_.load(
        std::memory_order_relaxed);
  }

  bool was_process_callback_pending_during_ipc_dispatch() const {
    return was_process_callback_pending_during_ipc_dispatch_;
  }

 private:
  // IPC::mojom::InterfaceWithUrgentMethod:
  void MaybeUrgentMessage(bool is_urgent) override {
    ++num_maybe_urgent_messages_received_;
    if (!is_urgent) {
      return;
    }
    ++num_urgent_messages_received_;
    uint32_t received =
        num_observer_urgent_messages_received_.load(std::memory_order_relaxed);
    uint32_t processed =
        num_observer_urgent_messages_processed_.load(std::memory_order_relaxed);
    // The "processed" observer callback should run after the IPC is dispatched,
    // so there should always be at least one less processed callback here.
    was_process_callback_pending_during_ipc_dispatch_ =
        was_process_callback_pending_during_ipc_dispatch_ &&
        (processed < received);
  }

  void NonUrgentMessage() override { ++num_non_urgent_messages_received_; }

  void RequestQuit(RequestQuitCallback callback) override {
    std::move(quit_closure_).Run();
    std::move(callback).Run();
  }

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    CHECK(!receiver_.is_bound());
    CHECK_EQ(interface_name, IPC::mojom::InterfaceWithUrgentMethod::Name_);
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::InterfaceWithUrgentMethod>(
            std::move(handle)));
  }

  // IPC::UrgentMessageObserver:
  void OnUrgentMessageReceived() override {
    std::atomic_fetch_add_explicit(&num_observer_urgent_messages_received_,
                                   uint32_t(1), std::memory_order_relaxed);
  }

  void OnUrgentMessageProcessed() override {
    std::atomic_fetch_add_explicit(&num_observer_urgent_messages_processed_,
                                   uint32_t(1), std::memory_order_relaxed);
  }

  base::OnceClosure quit_closure_;
  mojo::AssociatedReceiver<IPC::mojom::InterfaceWithUrgentMethod> receiver_{
      this};
  uint32_t num_maybe_urgent_messages_received_{0};
  uint32_t num_urgent_messages_received_{0};
  uint32_t num_non_urgent_messages_received_{0};
  std::atomic<uint32_t> num_observer_urgent_messages_received_{0};
  std::atomic<uint32_t> num_observer_urgent_messages_processed_{0};
  bool was_process_callback_pending_during_ipc_dispatch_{true};
};

TEST_F(IPCChannelProxyMojoTest, UrgentMessageObserver) {
  Init("UrgentMessageObserverClient");

  base::RunLoop run_loop;
  ListenerWithUrgentMessageAssociatedInterface listener(run_loop.QuitClosure());
  CreateProxy(&listener, /*urgent_message_observer=*/&listener);
  RunProxy();

  run_loop.Run();

  EXPECT_TRUE(WaitForClientShutdown());

  EXPECT_EQ(listener.num_maybe_urgent_messages_received(), 5u);
  EXPECT_EQ(listener.num_urgent_messages_received(), 3u);
  EXPECT_EQ(listener.num_non_urgent_messages_received(), 2u);

  EXPECT_EQ(listener.num_observer_urgent_messages_received(), 3u);
  EXPECT_EQ(listener.num_observer_urgent_messages_processed(), 3u);
  EXPECT_TRUE(listener.was_process_callback_pending_during_ipc_dispatch());

  DestroyProxy();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    UrgentMessageObserverClient,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::InterfaceWithUrgentMethod> remote;
  proxy()->GetRemoteAssociatedInterface(
      remote.BindNewEndpointAndPassReceiver());

  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }
  remote->NonUrgentMessage();
  remote->MaybeUrgentMessage(/*is_urgent=*/false);
  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }
  remote->NonUrgentMessage();
  remote->MaybeUrgentMessage(/*is_urgent=*/false);
  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }

  base::RunLoop run_loop;
  remote->RequestQuit(run_loop.QuitClosure());
  run_loop.Run();

  DestroyProxy();
}

}  // namespace
}  // namespace ipc_channel_mojo_unittest
