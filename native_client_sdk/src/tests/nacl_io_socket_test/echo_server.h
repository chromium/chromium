// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_SOCKET_TEST_ECHO_SERVER_H_
#define TESTS_NACL_IO_SOCKET_TEST_ECHO_SERVER_H_

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/utility/completion_callback_factory.h"

static const int kBufferSize = 1024;

// Simple "echo" server based on a listening pp::TCPSocket.
// This server handles just one connection at a time and will
// echo back whatever bytes get sent to it.
class EchoServer {
 typedef void (*LogFunction)(const char*);

 public:
  EchoServer(pp::Instance* instance,
             uint16_t port,
             LogFunction log_function=NULL,
             pthread_cond_t* ready_cond=NULL,
             pthread_mutex_t* ready_lock=NULL)
    : instance_(instance),
      callback_factory_(this),
      ready_(false),
      ready_cond_(ready_cond),
      ready_lock_(ready_lock),
      log_function_(log_function) {
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

  void Log(const char* msg) {
    if (log_function_)
      log_function_(msg);
  }

  pp::Instance* instance_;
  pp::CompletionCallbackFactory<EchoServer> callback_factory_;
  pp::TCPSocket listening_socket_;
  pp::TCPSocket incoming_socket_;

  char receive_buffer_[kBufferSize];
  bool ready_;
  pthread_cond_t* ready_cond_;
  pthread_mutex_t* ready_lock_;
  LogFunction log_function_;
};

#endif  // TESTS_NACL_IO_SOCKET_TEST_ECHO_SERVER_H_
