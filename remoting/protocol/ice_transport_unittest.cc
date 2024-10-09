// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/webrtc/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/message_channel_factory.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/transport_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using testing::_;

namespace remoting::protocol {

namespace {

// Send 100 messages 1024 bytes each. UDP messages are sent with 10ms delay
// between messages (about 1 second for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const char kChannelName[] = "test_channel";

ACTION_P2(QuitRunLoopOnCounter, run_loop, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0) {
    run_loop->Quit();
  }
}

class MockChannelCreatedCallback {
 public:
  MOCK_METHOD1(OnDone, void(MessagePipe* socket));
};

class TestTransportEventHandler : public IceTransport::EventHandler {
 public:
  typedef base::RepeatingCallback<void(ErrorCode error)> ErrorCallback;

  TestTransportEventHandler() = default;

  TestTransportEventHandler(const TestTransportEventHandler&) = delete;
  TestTransportEventHandler& operator=(const TestTransportEventHandler&) =
      delete;

  ~TestTransportEventHandler() = default;

  void set_error_callback(const ErrorCallback& callback) {
    error_callback_ = callback;
  }

  // IceTransport::EventHandler interface.
  void OnIceTransportRouteChange(const std::string& channel_name,
                                 const TransportRoute& route) override {}
  void OnIceTransportError(ErrorCode error) override {
    error_callback_.Run(error);
  }

 private:
  ErrorCallback error_callback_;
};

}  // namespace

class IceTransportTest : public testing::Test {
 public:
  IceTransportTest() {
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  void TearDown() override {
    client_message_pipe_.reset();
    host_message_pipe_.reset();
    client_transport_.reset();
    host_transport_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ProcessTransportInfo(
      std::unique_ptr<IceTransport>* target_transport,
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IceTransportTest::DeliverTransportInfo,
                       base::Unretained(this), target_transport,
                       std::move(transport_info)),
        transport_info_delay_);
  }

  void DeliverTransportInfo(
      std::unique_ptr<IceTransport>* target_transport,
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
    ASSERT_TRUE(target_transport);
    EXPECT_TRUE(
        (*target_transport)->ProcessTransportInfo(transport_info.get()));
  }

  void InitializeConnection() {
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

    rtc::SocketFactory* socket_factory =
        webrtc::ThreadWrapper::current()->SocketServer();
    host_transport_ = std::make_unique<IceTransport>(
        base::MakeRefCounted<TransportContext>(
            std::make_unique<ChromiumPortAllocatorFactory>(), socket_factory,
            /*ice_config_fetcher=*/nullptr, TransportRole::SERVER),
        &host_event_handler_);
    host_transport_->ApplyNetworkSettings(network_settings_);
    if (!host_authenticator_) {
      host_authenticator_ =
          std::make_unique<FakeAuthenticator>(FakeAuthenticator::ACCEPT);
    }

    client_transport_ = std::make_unique<IceTransport>(
        base::MakeRefCounted<TransportContext>(
            std::make_unique<ChromiumPortAllocatorFactory>(), socket_factory,
            /*ice_config_fetcher=*/nullptr, TransportRole::CLIENT),
        &client_event_handler_);
    client_transport_->ApplyNetworkSettings(network_settings_);
    if (!client_authenticator_) {
      client_authenticator_ =
          std::make_unique<FakeAuthenticator>(FakeAuthenticator::ACCEPT);
    }

    host_event_handler_.set_error_callback(base::BindRepeating(
        &IceTransportTest::OnTransportError, base::Unretained(this)));
    client_event_handler_.set_error_callback(base::BindRepeating(
        &IceTransportTest::OnTransportError, base::Unretained(this)));

    // Start both transports.
    host_transport_->Start(
        host_authenticator_.get(),
        base::BindRepeating(&IceTransportTest::ProcessTransportInfo,
                            base::Unretained(this), &client_transport_));
    client_transport_->Start(
        client_authenticator_.get(),
        base::BindRepeating(&IceTransportTest::ProcessTransportInfo,
                            base::Unretained(this), &host_transport_));
  }

  void WaitUntilConnected() {
    run_loop_ = std::make_unique<base::RunLoop>();

    int counter = 2;
    EXPECT_CALL(client_channel_callback_, OnDone(_))
        .WillOnce(QuitRunLoopOnCounter(run_loop_.get(), &counter));
    EXPECT_CALL(host_channel_callback_, OnDone(_))
        .WillOnce(QuitRunLoopOnCounter(run_loop_.get(), &counter));

    run_loop_->Run();

    EXPECT_TRUE(client_message_pipe_.get());
    EXPECT_TRUE(host_message_pipe_.get());
  }

  void OnClientChannelCreated(std::unique_ptr<MessagePipe> message_pipe) {
    client_message_pipe_ = std::move(message_pipe);
    client_channel_callback_.OnDone(client_message_pipe_.get());
  }

  void OnHostChannelCreated(std::unique_ptr<MessagePipe> message_pipe) {
    host_message_pipe_ = std::move(message_pipe);
    host_channel_callback_.OnDone(host_message_pipe_.get());
  }

  void OnTransportError(ErrorCode error) {
    LOG(ERROR) << "Transport Error";
    error_ = error;
    run_loop_->Quit();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<base::RunLoop> run_loop_;

  NetworkSettings network_settings_;

  base::TimeDelta transport_info_delay_;

  std::unique_ptr<IceTransport> host_transport_;
  TestTransportEventHandler host_event_handler_;
  std::unique_ptr<FakeAuthenticator> host_authenticator_;

  std::unique_ptr<IceTransport> client_transport_;
  TestTransportEventHandler client_event_handler_;
  std::unique_ptr<FakeAuthenticator> client_authenticator_;

  MockChannelCreatedCallback client_channel_callback_;
  MockChannelCreatedCallback host_channel_callback_;

  std::unique_ptr<MessagePipe> client_message_pipe_;
  std::unique_ptr<MessagePipe> host_message_pipe_;

  ErrorCode error_ = ErrorCode::OK;
};

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DataStream DISABLED_DataStream
#else
#define MAYBE_DataStream DataStream
#endif
TEST_F(IceTransportTest, MAYBE_DataStream) {
  InitializeConnection();

  client_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  host_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnHostChannelCreated,
                                   base::Unretained(this)));

  WaitUntilConnected();

  MessagePipeConnectionTester tester(host_message_pipe_.get(),
                                     client_message_pipe_.get(), kMessageSize,
                                     kMessages);
  tester.RunAndCheckResults();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MuxDataStream DISABLED_MuxDataStream
#else
#define MAYBE_MuxDataStream MuxDataStream
#endif
TEST_F(IceTransportTest, MAYBE_MuxDataStream) {
  InitializeConnection();

  client_transport_->GetMultiplexedChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  host_transport_->GetMultiplexedChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnHostChannelCreated,
                                   base::Unretained(this)));

  WaitUntilConnected();

  MessagePipeConnectionTester tester(host_message_pipe_.get(),
                                     client_message_pipe_.get(), kMessageSize,
                                     kMessages);
  tester.RunAndCheckResults();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FailedChannelAuth DISABLED_FailedChannelAuth
#else
#define MAYBE_FailedChannelAuth FailedChannelAuth
#endif
TEST_F(IceTransportTest, MAYBE_FailedChannelAuth) {
  // Use host authenticator with one that rejects channel authentication.
  host_authenticator_ =
      std::make_unique<FakeAuthenticator>(FakeAuthenticator::REJECT_CHANNEL);

  InitializeConnection();

  client_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  host_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnHostChannelCreated,
                                   base::Unretained(this)));

  run_loop_ = std::make_unique<base::RunLoop>();

  // The callback should never be called.
  EXPECT_CALL(host_channel_callback_, OnDone(_)).Times(0);

  run_loop_->Run();

  EXPECT_FALSE(host_message_pipe_);
  EXPECT_EQ(ErrorCode::CHANNEL_CONNECTION_ERROR, error_);

  client_transport_->GetChannelFactory()->CancelChannelCreation(kChannelName);
}

// Verify that channels are never marked connected if connection cannot be
// established.
TEST_F(IceTransportTest, TestBrokenTransport) {
  // Allow only incoming connections on both ends, which effectively renders
  // transport unusable. Also reduce connection timeout so the test finishes
  // quickly.
  network_settings_ = NetworkSettings(NetworkSettings::NAT_TRAVERSAL_DISABLED);
  network_settings_.ice_timeout = base::Seconds(1);
  network_settings_.ice_reconnect_attempts = 1;

  InitializeConnection();

  client_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  host_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnHostChannelCreated,
                                   base::Unretained(this)));

  // The RunLoop should quit in OnTransportError().
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  // Verify that neither of the two ends of the channel is connected.
  EXPECT_FALSE(client_message_pipe_);
  EXPECT_FALSE(host_message_pipe_);
  EXPECT_EQ(ErrorCode::CHANNEL_CONNECTION_ERROR, error_);

  client_transport_->GetChannelFactory()->CancelChannelCreation(kChannelName);
  host_transport_->GetChannelFactory()->CancelChannelCreation(kChannelName);
}

TEST_F(IceTransportTest, TestCancelChannelCreation) {
  InitializeConnection();

  client_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  client_transport_->GetChannelFactory()->CancelChannelCreation(kChannelName);

  EXPECT_TRUE(!client_message_pipe_.get());
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TestDelayedSignaling DISABLED_TestDelayedSignaling
#else
#define MAYBE_TestDelayedSignaling TestDelayedSignaling
#endif
// Verify that we can still connect even when there is a delay in signaling
// messages delivery.
TEST_F(IceTransportTest, MAYBE_TestDelayedSignaling) {
  transport_info_delay_ = base::Milliseconds(100);

  InitializeConnection();

  client_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnClientChannelCreated,
                                   base::Unretained(this)));
  host_transport_->GetChannelFactory()->CreateChannel(
      kChannelName, base::BindOnce(&IceTransportTest::OnHostChannelCreated,
                                   base::Unretained(this)));

  WaitUntilConnected();

  MessagePipeConnectionTester tester(host_message_pipe_.get(),
                                     client_message_pipe_.get(), kMessageSize,
                                     kMessages);
  tester.RunAndCheckResults();
}

}  // namespace remoting::protocol
