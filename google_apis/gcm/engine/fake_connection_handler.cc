// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/fake_connection_handler.h"

#include <utility>

#include "base/logging.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// Build a basic login response.
std::unique_ptr<google::protobuf::MessageLite> BuildLoginResponse(
    bool fail_login) {
  std::unique_ptr<mcs_proto::LoginResponse> login_response(
      new mcs_proto::LoginResponse());
  login_response->set_id("id");
  if (fail_login)
    login_response->mutable_error()->set_code(1);
  return std::move(login_response);
}

}  // namespace

FakeConnectionHandler::FakeConnectionHandler(
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback)
    : read_callback_(read_callback),
      write_callback_(write_callback),
      fail_login_(false),
      fail_send_(false),
      initialized_(false),
      had_error_(false) {
}

FakeConnectionHandler::~FakeConnectionHandler() {
}

void FakeConnectionHandler::Init(
    const mcs_proto::LoginRequest& login_request,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  ASSERT_GE(expected_outgoing_messages_.size(), 1U);
  EXPECT_EQ(expected_outgoing_messages_.front().SerializeAsString(),
            login_request.SerializeAsString());
  expected_outgoing_messages_.pop_front();
  DVLOG(1) << "Received init call.";
  read_callback_.Run(BuildLoginResponse(fail_login_));
  initialized_ = !fail_login_;
}

void FakeConnectionHandler::Reset() {
  initialized_ = false;
  had_error_ = false;
}

bool FakeConnectionHandler::CanSendMessage() const {
  return initialized_ && !had_error_;
}

void FakeConnectionHandler::SendMessage(
    const google::protobuf::MessageLite& message) {
  if (expected_outgoing_messages_.empty())
    FAIL() << "Unexpected message sent.";
  EXPECT_EQ(expected_outgoing_messages_.front().SerializeAsString(),
            message.SerializeAsString());
  expected_outgoing_messages_.pop_front();
  DVLOG(1) << "Received message, "
           << (fail_send_ ? " failing send." : "calling back.");
  if (!fail_send_)
    write_callback_.Run();
  else
    initialized_ = false;  // Prevent future messages until reconnect.
}

void FakeConnectionHandler::ExpectOutgoingMessage(const MCSMessage& message) {
  expected_outgoing_messages_.push_back(message);
}

void FakeConnectionHandler::ResetOutgoingMessageExpectations() {
  expected_outgoing_messages_.clear();
}

bool FakeConnectionHandler::AllOutgoingMessagesReceived() const {
  return expected_outgoing_messages_.empty();
}

void FakeConnectionHandler::ReceiveMessage(const MCSMessage& message) {
  read_callback_.Run(message.CloneProtobuf());
}

}  // namespace gcm
