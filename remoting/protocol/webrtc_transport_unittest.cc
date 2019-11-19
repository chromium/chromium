// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_transport.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/message_channel_factory.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

const char kChannelName[] = "test_channel";
const char kAuthKey[] = "test_auth_key";

class TestTransportEventHandler : public WebrtcTransport::EventHandler {
 public:
  typedef base::Callback<void(ErrorCode error)> ErrorCallback;
  typedef base::Callback<void(const std::string& name,
                              std::unique_ptr<MessagePipe> pipe)>
      IncomingChannelCallback;

  TestTransportEventHandler() = default;
  ~TestTransportEventHandler() override = default;

  // All callbacks must be set before the test handler is passed to a Transport
  // object.
  void set_connecting_callback(const base::Closure& callback) {
    connecting_callback_ = callback;
  }
  void set_connected_callback(const base::Closure& callback) {
    connected_callback_ = callback;
  }
  void set_error_callback(const ErrorCallback& callback) {
    error_callback_ = callback;
  }
  void set_incoming_channel_callback(const IncomingChannelCallback& callback) {
    incoming_channel_callback_ = callback;
  }

  // WebrtcTransport::EventHandler interface.
  void OnWebrtcTransportConnecting() override {
    if (!connecting_callback_.is_null())
      connecting_callback_.Run();
  }
  void OnWebrtcTransportConnected() override {
    if (!connected_callback_.is_null())
      connected_callback_.Run();
  }
  void OnWebrtcTransportError(ErrorCode error) override {
    error_callback_.Run(error);
  }
  void OnWebrtcTransportIncomingDataChannel(
      const std::string& name,
      std::unique_ptr<MessagePipe> pipe) override {
    if (!incoming_channel_callback_.is_null()) {
      incoming_channel_callback_.Run(name, std::move(pipe));
    } else {
      FAIL() << "Received unexpected incoming channel.";
    }
  }
  void OnWebrtcTransportMediaStreamAdded(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
  void OnWebrtcTransportMediaStreamRemoved(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override {}

 private:
  base::Closure connecting_callback_;
  base::Closure connected_callback_;
  ErrorCallback error_callback_;
  IncomingChannelCallback incoming_channel_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestTransportEventHandler);
};

class TestMessagePipeEventHandler : public MessagePipe::EventHandler {
 public:
  TestMessagePipeEventHandler() = default;
  ~TestMessagePipeEventHandler() override = default;

  void set_open_callback(const base::Closure& callback) {
    open_callback_ = callback;
  }
  void set_message_callback(const base::Closure& callback) {
    message_callback_ = callback;
  }
  void set_closed_callback(const base::Closure& callback) {
    closed_callback_ = callback;
  }

  bool is_open() { return is_open_; }
  const std::list<std::unique_ptr<CompoundBuffer>>& received_messages() {
    return received_messages_;
  }

  // MessagePipe::EventHandler interface.
  void OnMessagePipeOpen() override {
    is_open_ = true;
    if (!open_callback_.is_null())
      open_callback_.Run();
  }
  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override {
    received_messages_.push_back(std::move(message));
    if (!message_callback_.is_null())
      message_callback_.Run();
  }
  void OnMessagePipeClosed() override {
    if (!closed_callback_.is_null()) {
      closed_callback_.Run();
    } else {
      FAIL() << "Channel closed unexpectedly.";
    }
  }

 private:
  bool is_open_ = false;
  base::Closure open_callback_;
  base::Closure message_callback_;
  base::Closure closed_callback_;

  std::list<std::unique_ptr<CompoundBuffer>> received_messages_;

  DISALLOW_COPY_AND_ASSIGN(TestMessagePipeEventHandler);
};

}  // namespace

class WebrtcTransportTest : public testing::Test {
 public:
  WebrtcTransportTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  void TearDown() override {
    run_loop_.reset();
    client_message_pipe_.reset();
    client_transport_.reset();
    host_message_pipe_.reset();
    host_transport_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ProcessTransportInfo(std::unique_ptr<WebrtcTransport>* target_transport,
                            bool normalize_line_endings,
                            std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
    ASSERT_TRUE(target_transport);

    // Reformat the message to normalize line endings by removing CR symbol.
    if (normalize_line_endings) {
      std::string xml = transport_info->Str();
      base::ReplaceChars(xml, "\r", std::string(), &xml);
      transport_info.reset(jingle_xmpp::XmlElement::ForStr(xml));
    }

    EXPECT_TRUE(
        (*target_transport)->ProcessTransportInfo(transport_info.get()));
  }

  void InitializeConnection() {
    host_transport_.reset(
        new WebrtcTransport(jingle_glue::JingleThreadWrapper::current(),
                            TransportContext::ForTests(TransportRole::SERVER),
                            &host_event_handler_));
    host_authenticator_.reset(new FakeAuthenticator(FakeAuthenticator::ACCEPT));
    host_authenticator_->set_auth_key(kAuthKey);

    client_transport_.reset(
        new WebrtcTransport(jingle_glue::JingleThreadWrapper::current(),
                            TransportContext::ForTests(TransportRole::CLIENT),
                            &client_event_handler_));
    client_authenticator_.reset(
        new FakeAuthenticator(FakeAuthenticator::ACCEPT));
    client_authenticator_->set_auth_key(kAuthKey);
  }

  void StartConnection() {
    host_event_handler_.set_connected_callback(base::DoNothing());
    client_event_handler_.set_connected_callback(base::DoNothing());

    host_event_handler_.set_error_callback(
        base::Bind(&WebrtcTransportTest::OnSessionError, base::Unretained(this),
                   TransportRole::SERVER));
    client_event_handler_.set_error_callback(
        base::Bind(&WebrtcTransportTest::OnSessionError, base::Unretained(this),
                   TransportRole::CLIENT));

    // Start both transports.
    host_transport_->Start(
        host_authenticator_.get(),
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &client_transport_, true));
    client_transport_->Start(
        client_authenticator_.get(),
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &host_transport_, false));
  }

  void WaitUntilConnected() {
    int counter = 2;
    host_event_handler_.set_connected_callback(
        base::Bind(&WebrtcTransportTest::QuitRunLoopOnCounter,
                   base::Unretained(this), &counter));
    client_event_handler_.set_connected_callback(
        base::Bind(&WebrtcTransportTest::QuitRunLoopOnCounter,
                   base::Unretained(this), &counter));

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();

    host_event_handler_.set_connected_callback(base::Closure());
    client_event_handler_.set_connected_callback(base::Closure());

    EXPECT_EQ(OK, client_error_);
    EXPECT_EQ(OK, host_error_);
  }

  void ExpectClientDataStream() {
    client_event_handler_.set_incoming_channel_callback(base::Bind(
        &WebrtcTransportTest::OnIncomingChannel, base::Unretained(this)));
  }

  void CreateHostDataStream() {
    host_message_pipe_ = host_transport_->CreateOutgoingChannel(kChannelName);
    host_message_pipe_->Start(&host_message_pipe_event_handler_);
    host_message_pipe_event_handler_.set_open_callback(base::Bind(
        &WebrtcTransportTest::OnHostChannelConnected, base::Unretained(this)));
  }

  void OnIncomingChannel(const std::string& name,
                         std::unique_ptr<MessagePipe> pipe) {
    EXPECT_EQ(kChannelName, name);
    client_message_pipe_ = std::move(pipe);
    client_message_pipe_->Start(&client_message_pipe_event_handler_);

    if (run_loop_ && host_message_pipe_event_handler_.is_open())
      run_loop_->Quit();
  }

  void OnHostChannelConnected() {
    if (run_loop_ && client_message_pipe_event_handler_.is_open())
      run_loop_->Quit();
  }

  void OnSessionError(TransportRole role, ErrorCode error) {
    if (role == TransportRole::SERVER) {
      host_error_ = error;
      if (destroy_on_error_) {
        host_message_pipe_.reset();
        host_transport_.reset();
      }
    } else {
      CHECK(role == TransportRole::CLIENT);
      client_error_ = error;
      if (destroy_on_error_) {
        client_message_pipe_.reset();
        client_transport_.reset();
      }
    }
    run_loop_->Quit();
  }

  void OnHostChannelClosed() {
    host_message_pipe_.reset();
    run_loop_->Quit();
  }

  void QuitRunLoopOnCounter(int* counter) {
    --(*counter);
    if (*counter == 0)
      run_loop_->Quit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  NetworkSettings network_settings_;

  std::unique_ptr<WebrtcTransport> host_transport_;
  TestTransportEventHandler host_event_handler_;
  std::unique_ptr<FakeAuthenticator> host_authenticator_;

  std::unique_ptr<WebrtcTransport> client_transport_;
  TestTransportEventHandler client_event_handler_;
  std::unique_ptr<FakeAuthenticator> client_authenticator_;

  std::unique_ptr<MessagePipe> client_message_pipe_;
  TestMessagePipeEventHandler client_message_pipe_event_handler_;
  std::unique_ptr<MessagePipe> host_message_pipe_;
  TestMessagePipeEventHandler host_message_pipe_event_handler_;

  ErrorCode client_error_ = OK;
  ErrorCode host_error_ = OK;

  bool destroy_on_error_ = false;
};

TEST_F(WebrtcTransportTest, Connects) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();
}

TEST_F(WebrtcTransportTest, InvalidAuthKey) {
  InitializeConnection();
  client_authenticator_->set_auth_key("Incorrect Key");
  StartConnection();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_EQ(AUTHENTICATION_FAILED, client_error_);
}

TEST_F(WebrtcTransportTest, DataStream) {
  client_event_handler_.set_connecting_callback(base::Bind(
      &WebrtcTransportTest::ExpectClientDataStream, base::Unretained(this)));
  host_event_handler_.set_connecting_callback(base::Bind(
      &WebrtcTransportTest::CreateHostDataStream, base::Unretained(this)));

  InitializeConnection();
  StartConnection();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_TRUE(client_message_pipe_);
  EXPECT_TRUE(host_message_pipe_);

  TextEvent message;
  message.set_text("Hello");
  host_message_pipe_->Send(&message, base::Closure());

  run_loop_.reset(new base::RunLoop());
  client_message_pipe_event_handler_.set_message_callback(
      base::Bind(&base::RunLoop::Quit, base::Unretained(run_loop_.get())));
  run_loop_->Run();

  ASSERT_EQ(1U, client_message_pipe_event_handler_.received_messages().size());

  std::unique_ptr<TextEvent> received_message = ParseMessage<TextEvent>(
      client_message_pipe_event_handler_.received_messages().front().get());
  EXPECT_EQ(message.text(), received_message->text());
}

// Verify that data streams can be created after connection has been initiated.
TEST_F(WebrtcTransportTest, DataStreamLate) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();

  ExpectClientDataStream();
  CreateHostDataStream();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_TRUE(client_message_pipe_);
  EXPECT_TRUE(host_message_pipe_);
}

TEST_F(WebrtcTransportTest, TerminateDataChannel) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();

  ExpectClientDataStream();
  CreateHostDataStream();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_TRUE(client_message_pipe_);
  EXPECT_TRUE(host_message_pipe_);

  destroy_on_error_ = true;

  // Expect that the channel is closed on the host side once the client closes
  // the channel.
  host_message_pipe_event_handler_.set_closed_callback(base::Bind(
      &WebrtcTransportTest::OnHostChannelClosed, base::Unretained(this)));

  // Destroy pipe on one side of the of the connection. It should get closed on
  // the other side.
  client_message_pipe_.reset();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  // Check that OnHostChannelClosed() has been called.
  EXPECT_EQ(OK, host_error_);
  EXPECT_FALSE(host_message_pipe_);
}

}  // namespace protocol
}  // namespace remoting
