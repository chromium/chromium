// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_HANDLER_H_

#include <list>

#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace gcm {

// A fake implementation of a ConnectionHandler that can arbitrarily receive
// messages and verify expectations for outgoing messages.
class FakeConnectionHandler : public ConnectionHandler {
 public:
  FakeConnectionHandler(
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback,
      const ConnectionHandler::ConnectionChangedCallback& connection_callback);

  FakeConnectionHandler(const FakeConnectionHandler&) = delete;
  FakeConnectionHandler& operator=(const FakeConnectionHandler&) = delete;

  ~FakeConnectionHandler() override;

  // ConnectionHandler implementation.
  void Init(const mcs_proto::LoginRequest& login_request,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream) override;
  void Reset() override;
  bool CanSendMessage() const override;
  void SendMessage(const google::protobuf::MessageLite& message) override;

  // EXPECT's receipt of |message| via SendMessage(..).
  void ExpectOutgoingMessage(const MCSMessage& message);

  // Reset the expected outgoing messages.
  void ResetOutgoingMessageExpectations();

  // Whether all expected outgoing messages have been received;
  bool AllOutgoingMessagesReceived() const;

  // Passes on |message| to |write_callback_|.
  void ReceiveMessage(const MCSMessage& message);

  // Whether to return an error with the next login response.
  void set_fail_login(bool fail_login) {
    fail_login_ = fail_login;
  }

  // Whether to invoke the write callback on the next send attempt or fake a
  // connection error instead.
  void set_fail_send(bool fail_send) {
    fail_send_ = fail_send;
  }

  // Whether a socket-level error was encountered or not.
  void set_had_error(bool had_error) {
    had_error_ = had_error;
  }

  bool initialized() const { return initialized_; }

 private:
  ConnectionHandler::ProtoReceivedCallback read_callback_;
  ConnectionHandler::ProtoSentCallback write_callback_;
  ConnectionHandler::ConnectionChangedCallback connection_callback_;

  std::list<MCSMessage> expected_outgoing_messages_;

  // Whether to fail the login or not.
  bool fail_login_;

  // Whether to fail a SendMessage call or not.
  bool fail_send_;

  // Whether a successful login has completed.
  bool initialized_;

  // Whether an error was encountered after a successful login.
  bool had_error_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_HANDLER_H_
