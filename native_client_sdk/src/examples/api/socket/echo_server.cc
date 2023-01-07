// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "echo_server.h"

#include <string.h>
#include <sstream>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
#endif

// Number of connections to queue up on the listening
// socket before new ones get "Connection Refused"
static const int kBacklog = 10;

// Implement htons locally.  Even though this is provided by
// nacl_io we don't want to include nacl_io in this simple
// example.
static uint16_t Htons(uint16_t hostshort) {
  uint8_t result_bytes[2];
  result_bytes[0] = (uint8_t) ((hostshort >> 8) & 0xFF);
  result_bytes[1] = (uint8_t) (hostshort & 0xFF);

  uint16_t result;
  memcpy(&result, result_bytes, 2);
  return result;
}

void EchoServer::Start(uint16_t port) {
  if (!pp::TCPSocket::IsAvailable()) {
    instance_->PostMessage("TCPSocket not available");
    return;
  }

  listening_socket_ = pp::TCPSocket(instance_);
  if (listening_socket_.is_null()) {
    instance_->PostMessage("Error creating TCPSocket.");
    return;
  }

  std::ostringstream status;
  status << "Starting server on port: " << port;
  instance_->PostMessage(status.str());

  // Attempt to listen on all interfaces (0.0.0.0)
  // on the given port number.
  PP_NetAddress_IPv4 ipv4_addr = { Htons(port), { 0 } };
  pp::NetAddress addr(instance_, ipv4_addr);
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&EchoServer::OnBindCompletion);
  int32_t rtn = listening_socket_.Bind(addr, callback);
  if (rtn != PP_OK_COMPLETIONPENDING) {
    instance_->PostMessage("Error binding listening socket.");
    return;
  }
}

void EchoServer::OnBindCompletion(int32_t result) {
  if (result != PP_OK) {
    std::ostringstream status;
    status << "server: Bind failed with: " << result;
    instance_->PostMessage(status.str());
    return;
  }

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&EchoServer::OnListenCompletion);

  int32_t rtn = listening_socket_.Listen(kBacklog, callback);
  if (rtn != PP_OK_COMPLETIONPENDING) {
    instance_->PostMessage("server: Error listening on server socket.");
    return;
  }
}

void EchoServer::OnListenCompletion(int32_t result) {
  std::ostringstream status;
  if (result != PP_OK) {
    status << "server: Listen failed with: " << result;
    instance_->PostMessage(status.str());
    return;
  }

  pp::NetAddress addr = listening_socket_.GetLocalAddress();
  status << "server: Listening on: " << addr.DescribeAsString(true).AsString();
  instance_->PostMessage(status.str());

  TryAccept();
}

void EchoServer::OnAcceptCompletion(int32_t result, pp::TCPSocket socket) {
  std::ostringstream status;

  if (result != PP_OK) {
    status << "server: Accept failed: " << result;
    instance_->PostMessage(status.str());
    return;
  }

  pp::NetAddress addr = socket.GetLocalAddress();
  status << "server: New connection from: ";
  status << addr.DescribeAsString(true).AsString();
  instance_->PostMessage(status.str());
  incoming_socket_ = socket;

  TryRead();
}

void EchoServer::OnReadCompletion(int32_t result) {
  std::ostringstream status;
  if (result <= 0) {
    if (result == 0)
      status << "server: client disconnected";
    else
      status << "server: Read failed: " << result;
    instance_->PostMessage(status.str());

    // Remove the current incoming socket and try
    // to accept the next one.
    incoming_socket_.Close();
    incoming_socket_ = pp::TCPSocket();
    TryAccept();
    return;
  }

  status << "server: Read " << result << " bytes";
  instance_->PostMessage(status.str());

  // Echo the bytes back to the client
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&EchoServer::OnWriteCompletion);
  result = incoming_socket_.Write(receive_buffer_, result, callback);
  if (result != PP_OK_COMPLETIONPENDING) {
    status << "server: Write failed: " << result;
    instance_->PostMessage(status.str());
  }
}

void EchoServer::OnWriteCompletion(int32_t result) {
  std::ostringstream status;
  if (result < 0) {
    status << "server: Write failed: " << result;
    instance_->PostMessage(status.str());
    return;
  }

  status << "server: Wrote " << result << " bytes";
  instance_->PostMessage(status.str());

  // Try and read more bytes from the client
  TryRead();
}

void EchoServer::TryRead() {
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&EchoServer::OnReadCompletion);
  incoming_socket_.Read(receive_buffer_, kBufferSize, callback);
}

void EchoServer::TryAccept() {
  pp::CompletionCallbackWithOutput<pp::TCPSocket> callback =
      callback_factory_.NewCallbackWithOutput(
          &EchoServer::OnAcceptCompletion);
  listening_socket_.Accept(callback);
}
