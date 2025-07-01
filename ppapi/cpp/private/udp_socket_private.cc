// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/udp_socket_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UDPSocket_Private_0_4>() {
  return PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4;
}

template <> const char* interface_name<PPB_UDPSocket_Private_0_3>() {
  return PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3;
}

}  // namespace

UDPSocketPrivate::UDPSocketPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_Private_0_4>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_Private_0_3>()->Create(
        instance.pp_instance()));
  }
}

// static
bool UDPSocketPrivate::IsAvailable() {
  return has_interface<PPB_UDPSocket_Private_0_4>() ||
      has_interface<PPB_UDPSocket_Private_0_3>();
}

int32_t UDPSocketPrivate::SetSocketFeature(PP_UDPSocketFeature_Private name,
                                           const Var& value) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    return get_interface<PPB_UDPSocket_Private_0_4>()->SetSocketFeature(
        pp_resource(), name, value.pp_var());
  }
  return PP_ERROR_NOINTERFACE;
}

int32_t UDPSocketPrivate::Bind(const PP_NetAddress_Private* addr,
                               const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    return get_interface<PPB_UDPSocket_Private_0_4>()->Bind(
        pp_resource(), addr, callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    return get_interface<PPB_UDPSocket_Private_0_3>()->Bind(
        pp_resource(), addr, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool UDPSocketPrivate::GetBoundAddress(PP_NetAddress_Private* addr) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    PP_Bool result =
        get_interface<PPB_UDPSocket_Private_0_4>()->GetBoundAddress(
            pp_resource(), addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    PP_Bool result =
        get_interface<PPB_UDPSocket_Private_0_3>()->GetBoundAddress(
            pp_resource(), addr);
    return PP_ToBool(result);
  }
  return false;
}

int32_t UDPSocketPrivate::RecvFrom(char* buffer,
                                   int32_t num_bytes,
                                   const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    return get_interface<PPB_UDPSocket_Private_0_4>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    return get_interface<PPB_UDPSocket_Private_0_3>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool UDPSocketPrivate::GetRecvFromAddress(PP_NetAddress_Private* addr) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    PP_Bool result =
        get_interface<PPB_UDPSocket_Private_0_4>()->GetRecvFromAddress(
            pp_resource(), addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    PP_Bool result =
        get_interface<PPB_UDPSocket_Private_0_3>()->GetRecvFromAddress(
            pp_resource(), addr);
    return PP_ToBool(result);
  }
  return false;
}

int32_t UDPSocketPrivate::SendTo(const char* buffer,
                                 int32_t num_bytes,
                                 const PP_NetAddress_Private* addr,
                                 const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Private_0_4>()) {
    return get_interface<PPB_UDPSocket_Private_0_4>()->SendTo(
        pp_resource(), buffer, num_bytes, addr,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    return get_interface<PPB_UDPSocket_Private_0_3>()->SendTo(
        pp_resource(), buffer, num_bytes, addr,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void UDPSocketPrivate::Close() {
  if (has_interface<PPB_UDPSocket_Private_0_4>())
    return get_interface<PPB_UDPSocket_Private_0_4>()->Close(pp_resource());
  if (has_interface<PPB_UDPSocket_Private_0_3>())
    return get_interface<PPB_UDPSocket_Private_0_3>()->Close(pp_resource());
}

}  // namespace pp
