// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/udp_socket.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UDPSocket_1_0>() {
  return PPB_UDPSOCKET_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_UDPSocket_1_1>() {
  return PPB_UDPSOCKET_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_UDPSocket_1_2>() {
  return PPB_UDPSOCKET_INTERFACE_1_2;
}

}  // namespace

UDPSocket::UDPSocket() {
}

UDPSocket::UDPSocket(const InstanceHandle& instance) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_1_2>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_UDPSocket_1_1>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_1_1>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_UDPSocket_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_1_0>()->Create(
        instance.pp_instance()));
  }
}

UDPSocket::UDPSocket(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

UDPSocket::UDPSocket(const UDPSocket& other) : Resource(other) {
}

UDPSocket::~UDPSocket() {
}

UDPSocket& UDPSocket::operator=(const UDPSocket& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool UDPSocket::IsAvailable() {
  return has_interface<PPB_UDPSocket_1_2>() ||
         has_interface<PPB_UDPSocket_1_1>() ||
         has_interface<PPB_UDPSocket_1_0>();
}

int32_t UDPSocket::Bind(const NetAddress& addr,
                        const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->Bind(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_1>()) {
    return get_interface<PPB_UDPSocket_1_1>()->Bind(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_0>()) {
    return get_interface<PPB_UDPSocket_1_0>()->Bind(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

NetAddress UDPSocket::GetBoundAddress() {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_UDPSocket_1_2>()->GetBoundAddress(pp_resource()));
  }
  if (has_interface<PPB_UDPSocket_1_1>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_UDPSocket_1_1>()->GetBoundAddress(pp_resource()));
  }
  if (has_interface<PPB_UDPSocket_1_0>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_UDPSocket_1_0>()->GetBoundAddress(pp_resource()));
  }
  return NetAddress();
}

int32_t UDPSocket::RecvFrom(
    char* buffer,
    int32_t num_bytes,
    const CompletionCallbackWithOutput<NetAddress>& callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.output(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_1>()) {
    return get_interface<PPB_UDPSocket_1_1>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.output(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_0>()) {
    return get_interface<PPB_UDPSocket_1_0>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.output(),
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t UDPSocket::SendTo(const char* buffer,
                          int32_t num_bytes,
                          const NetAddress& addr,
                          const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->SendTo(
        pp_resource(), buffer, num_bytes, addr.pp_resource(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_1>()) {
    return get_interface<PPB_UDPSocket_1_1>()->SendTo(
        pp_resource(), buffer, num_bytes, addr.pp_resource(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_0>()) {
    return get_interface<PPB_UDPSocket_1_0>()->SendTo(
        pp_resource(), buffer, num_bytes, addr.pp_resource(),
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void UDPSocket::Close() {
  if (has_interface<PPB_UDPSocket_1_2>())
    return get_interface<PPB_UDPSocket_1_2>()->Close(pp_resource());
  if (has_interface<PPB_UDPSocket_1_1>())
    return get_interface<PPB_UDPSocket_1_1>()->Close(pp_resource());
  if (has_interface<PPB_UDPSocket_1_0>())
    return get_interface<PPB_UDPSocket_1_0>()->Close(pp_resource());
}

int32_t UDPSocket::SetOption(PP_UDPSocket_Option name,
                             const Var& value,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_1>()) {
    return get_interface<PPB_UDPSocket_1_1>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_1_0>()) {
    return get_interface<PPB_UDPSocket_1_0>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t UDPSocket::JoinGroup(const NetAddress& group,
                             const CompletionCallback callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->JoinGroup(
        pp_resource(), group.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t UDPSocket::LeaveGroup(const NetAddress& group,
                             const CompletionCallback callback) {
  if (has_interface<PPB_UDPSocket_1_2>()) {
    return get_interface<PPB_UDPSocket_1_2>()->LeaveGroup(
        pp_resource(), group.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
