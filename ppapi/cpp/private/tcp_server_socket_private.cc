// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/tcp_server_socket_private.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPServerSocket_Private_0_2>() {
  return PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_2;
}

template <> const char* interface_name<PPB_TCPServerSocket_Private_0_1>() {
  return PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1;
}

}  // namespace

TCPServerSocketPrivate::TCPServerSocketPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_TCPServerSocket_Private_0_2>()) {
    PassRefFromConstructor(
        get_interface<PPB_TCPServerSocket_Private_0_2>()->Create(
            instance.pp_instance()));
  } else if (has_interface<PPB_TCPServerSocket_Private_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_TCPServerSocket_Private_0_1>()->Create(
            instance.pp_instance()));
  }
}

// static
bool TCPServerSocketPrivate::IsAvailable() {
  return has_interface<PPB_TCPServerSocket_Private_0_2>() ||
      has_interface<PPB_TCPServerSocket_Private_0_1>();
}

int32_t TCPServerSocketPrivate::Listen(const PP_NetAddress_Private* addr,
                                       int32_t backlog,
                                       const CompletionCallback& callback) {
  if (has_interface<PPB_TCPServerSocket_Private_0_2>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_2>()->Listen(
        pp_resource(), addr, backlog, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPServerSocket_Private_0_1>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_1>()->Listen(
        pp_resource(), addr, backlog, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPServerSocketPrivate::Accept(PP_Resource* tcp_socket,
                                       const CompletionCallback& callback) {
  if (has_interface<PPB_TCPServerSocket_Private_0_2>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_2>()->Accept(
        pp_resource(), tcp_socket, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPServerSocket_Private_0_1>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_1>()->Accept(
        pp_resource(), tcp_socket, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPServerSocketPrivate::GetLocalAddress(PP_NetAddress_Private* addr) {
  if (has_interface<PPB_TCPServerSocket_Private_0_2>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_2>()->GetLocalAddress(
        pp_resource(), addr);
  }
  return PP_ERROR_NOINTERFACE;
}

void TCPServerSocketPrivate::StopListening() {
  if (has_interface<PPB_TCPServerSocket_Private_0_2>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_2>()->StopListening(
        pp_resource());
  }
  if (has_interface<PPB_TCPServerSocket_Private_0_1>()) {
    return get_interface<PPB_TCPServerSocket_Private_0_1>()->StopListening(
        pp_resource());
  }
}

}  // namespace pp
