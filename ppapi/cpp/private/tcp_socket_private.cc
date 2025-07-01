// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/tcp_socket_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPSocket_Private_0_5>() {
  return PPB_TCPSOCKET_PRIVATE_INTERFACE_0_5;
}

template <> const char* interface_name<PPB_TCPSocket_Private_0_4>() {
  return PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4;
}

template <> const char* interface_name<PPB_TCPSocket_Private_0_3>() {
  return PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3;
}

}  // namespace

TCPSocketPrivate::TCPSocketPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_Private_0_5>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_Private_0_4>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_Private_0_3>()->Create(
        instance.pp_instance()));
  }
}

TCPSocketPrivate::TCPSocketPrivate(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

// static
bool TCPSocketPrivate::IsAvailable() {
  return has_interface<PPB_TCPSocket_Private_0_5>() ||
      has_interface<PPB_TCPSocket_Private_0_4>() ||
      has_interface<PPB_TCPSocket_Private_0_3>();
}

int32_t TCPSocketPrivate::Connect(const char* host,
                                  uint16_t port,
                                  const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->Connect(
        pp_resource(), host, port, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->Connect(
        pp_resource(), host, port, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->Connect(
        pp_resource(), host, port, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPSocketPrivate::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->ConnectWithNetAddress(
        pp_resource(), addr, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->ConnectWithNetAddress(
        pp_resource(), addr, callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->ConnectWithNetAddress(
        pp_resource(), addr, callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

bool TCPSocketPrivate::GetLocalAddress(PP_NetAddress_Private* local_addr) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_5>()->
        GetLocalAddress(pp_resource(), local_addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_4>()->
        GetLocalAddress(pp_resource(), local_addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_3>()->
        GetLocalAddress(pp_resource(), local_addr);
    return PP_ToBool(result);
  }
  return false;
}

bool TCPSocketPrivate::GetRemoteAddress(PP_NetAddress_Private* remote_addr) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_5>()->
        GetRemoteAddress(pp_resource(), remote_addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_4>()->
        GetRemoteAddress(pp_resource(), remote_addr);
    return PP_ToBool(result);
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    PP_Bool result = get_interface<PPB_TCPSocket_Private_0_3>()->
        GetRemoteAddress(pp_resource(), remote_addr);
    return PP_ToBool(result);
  }
  return false;
}

int32_t TCPSocketPrivate::SSLHandshake(const char* server_name,
                                       uint16_t server_port,
                                       const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->SSLHandshake(
        pp_resource(), server_name, server_port,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->SSLHandshake(
        pp_resource(), server_name, server_port,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->SSLHandshake(
        pp_resource(), server_name, server_port,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

X509CertificatePrivate TCPSocketPrivate::GetServerCertificate() {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return X509CertificatePrivate(PASS_REF,
        get_interface<PPB_TCPSocket_Private_0_5>()->GetServerCertificate(
            pp_resource()));
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return X509CertificatePrivate(PASS_REF,
        get_interface<PPB_TCPSocket_Private_0_4>()->GetServerCertificate(
            pp_resource()));
  }
  return X509CertificatePrivate();
}

bool TCPSocketPrivate::AddChainBuildingCertificate(
    const X509CertificatePrivate& cert,
    bool trusted) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return PP_ToBool(get_interface<PPB_TCPSocket_Private_0_5>()->
        AddChainBuildingCertificate(pp_resource(), cert.pp_resource(),
                                    PP_FromBool(trusted)));
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return PP_ToBool(get_interface<PPB_TCPSocket_Private_0_4>()->
        AddChainBuildingCertificate(pp_resource(), cert.pp_resource(),
                                    PP_FromBool(trusted)));
  }
  return false;
}

int32_t TCPSocketPrivate::Read(char* buffer,
                               int32_t bytes_to_read,
                               const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->Read(
        pp_resource(), buffer, bytes_to_read,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->Read(
        pp_resource(), buffer, bytes_to_read,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->Read(
        pp_resource(), buffer, bytes_to_read,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPSocketPrivate::Write(const char* buffer,
                                int32_t bytes_to_write,
                                const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->Write(
        pp_resource(), buffer, bytes_to_write,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->Write(
        pp_resource(), buffer, bytes_to_write,
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->Write(
        pp_resource(), buffer, bytes_to_write,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void TCPSocketPrivate::Disconnect() {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->Disconnect(
        pp_resource());
  }
  if (has_interface<PPB_TCPSocket_Private_0_4>()) {
    return get_interface<PPB_TCPSocket_Private_0_4>()->Disconnect(
        pp_resource());
  }
  if (has_interface<PPB_TCPSocket_Private_0_3>()) {
    return get_interface<PPB_TCPSocket_Private_0_3>()->Disconnect(
        pp_resource());
  }
}

int32_t TCPSocketPrivate::SetOption(PP_TCPSocketOption_Private name,
                                    const Var& value,
                                    const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Private_0_5>()) {
    return get_interface<PPB_TCPSocket_Private_0_5>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
