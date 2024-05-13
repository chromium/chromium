// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/fake_connection_factory.h"

#include <memory>

#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace gcm {

FakeConnectionFactory::FakeConnectionFactory()
    : reconnect_pending_(false),
      delay_reconnect_(false),
      connection_listener_(nullptr) {
}

FakeConnectionFactory::~FakeConnectionFactory() {
}

void FakeConnectionFactory::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  request_builder_ = request_builder;
  connection_handler_ = std::make_unique<FakeConnectionHandler>(
      read_callback, write_callback,
      ConnectionHandler::ConnectionChangedCallback());
}

ConnectionHandler* FakeConnectionFactory::GetConnectionHandler() const {
  return connection_handler_.get();
}

void FakeConnectionFactory::Connect() {
  mcs_proto::LoginRequest login_request;
  request_builder_.Run(&login_request);
  connection_handler_->Init(login_request, mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
}

bool FakeConnectionFactory::IsEndpointReachable() const {
  return connection_handler_.get() && connection_handler_->CanSendMessage();
}

std::string FakeConnectionFactory::GetConnectionStateString() const {
  return "";
}

base::TimeTicks FakeConnectionFactory::NextRetryAttempt() const {
  return base::TimeTicks();
}

void FakeConnectionFactory::SignalConnectionReset(
    ConnectionResetReason reason) {
  if (!delay_reconnect_)
    Connect();
  else
    reconnect_pending_ = true;
  if (connection_listener_)
    connection_listener_->OnDisconnected();
}

void FakeConnectionFactory::SetConnectionListener(
    ConnectionListener* listener) {
  connection_listener_ = listener;
}

}  // namespace gcm
