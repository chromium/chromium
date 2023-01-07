// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_API_SOCKET_ECHO_SERVER_H_
#define EXAMPLES_API_SOCKET_ECHO_SERVER_H_

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/utility/completion_callback_factory.h"

static const int kBufferSize = 1024;

// Simple "echo" server based on a listening pp::TCPSocket.
// This server handles just one connection at a time and will
// echo back whatever bytes get sent to it.
class EchoServer {
 public:
  EchoServer(pp::Instance* instance, uint16_t port)
    : instance_(instance),
      callback_factory_(this) {
    Start(port);
  }

 protected:
  void Start(uint16_t port);

  // Callback functions
  void OnBindCompletion(int32_t result);
  void OnListenCompletion(int32_t result);
  void OnAcceptCompletion(int32_t result, pp::TCPSocket socket);
  void OnReadCompletion(int32_t result);
  void OnWriteCompletion(int32_t result);

  void TryRead();
  void TryAccept();

  pp::Instance* instance_;
  pp::CompletionCallbackFactory<EchoServer> callback_factory_;
  pp::TCPSocket listening_socket_;
  pp::TCPSocket incoming_socket_;

  char receive_buffer_[kBufferSize];
};

#endif  // EXAMPLES_API_SOCKET_ECHO_SERVER_H_
