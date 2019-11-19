// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_resource_base.h"

#include <cstring>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/tcp_socket_resource_constants.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_x509_certificate_private_api.h"

namespace ppapi {
namespace proxy {

TCPSocketResourceBase::TCPSocketResourceBase(Connection connection,
                                             PP_Instance instance,
                                             TCPSocketVersion version)
    : PluginResource(connection, instance),
      state_(TCPSocketState::INITIAL),
      read_buffer_(NULL),
      bytes_to_read_(-1),
      accepted_tcp_socket_(NULL),
      version_(version) {
  local_addr_.size = 0;
  memset(local_addr_.data, 0,
         base::size(local_addr_.data) * sizeof(*local_addr_.data));
  remote_addr_.size = 0;
  memset(remote_addr_.data, 0,
         base::size(remote_addr_.data) * sizeof(*remote_addr_.data));
}

TCPSocketResourceBase::TCPSocketResourceBase(
    Connection connection,
    PP_Instance instance,
    TCPSocketVersion version,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr)
    : PluginResource(connection, instance),
      state_(TCPSocketState::CONNECTED),
      read_buffer_(NULL),
      bytes_to_read_(-1),
      local_addr_(local_addr),
      remote_addr_(remote_addr),
      accepted_tcp_socket_(NULL),
      version_(version) {
}

TCPSocketResourceBase::~TCPSocketResourceBase() {
  CloseImpl();
}

int32_t TCPSocketResourceBase::BindImpl(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (state_.IsPending(TCPSocketState::BIND))
    return PP_ERROR_INPROGRESS;
  if (!state_.IsValidTransition(TCPSocketState::BIND))
    return PP_ERROR_FAILED;

  bind_callback_ = callback;
  state_.SetPendingTransition(TCPSocketState::BIND);

  Call<PpapiPluginMsg_TCPSocket_BindReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Bind(*addr),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgBindReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::ConnectImpl(
    const char* host,
    uint16_t port,
    scoped_refptr<TrackedCallback> callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;
  if (state_.IsPending(TCPSocketState::CONNECT))
    return PP_ERROR_INPROGRESS;
  if (!state_.IsValidTransition(TCPSocketState::CONNECT))
    return PP_ERROR_FAILED;

  connect_callback_ = callback;
  state_.SetPendingTransition(TCPSocketState::CONNECT);

  Call<PpapiPluginMsg_TCPSocket_ConnectReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Connect(host, port),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgConnectReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::ConnectWithNetAddressImpl(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (state_.IsPending(TCPSocketState::CONNECT))
    return PP_ERROR_INPROGRESS;
  if (!state_.IsValidTransition(TCPSocketState::CONNECT))
    return PP_ERROR_FAILED;

  connect_callback_ = callback;
  state_.SetPendingTransition(TCPSocketState::CONNECT);

  Call<PpapiPluginMsg_TCPSocket_ConnectReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_ConnectWithNetAddress(*addr),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgConnectReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool TCPSocketResourceBase::GetLocalAddressImpl(
    PP_NetAddress_Private* local_addr) {
  if (!state_.IsBound() || !local_addr)
    return PP_FALSE;
  *local_addr = local_addr_;
  return PP_TRUE;
}

PP_Bool TCPSocketResourceBase::GetRemoteAddressImpl(
    PP_NetAddress_Private* remote_addr) {
  if (!state_.IsConnected() || !remote_addr)
    return PP_FALSE;
  *remote_addr = remote_addr_;
  return PP_TRUE;
}

int32_t TCPSocketResourceBase::SSLHandshakeImpl(
    const char* server_name,
    uint16_t server_port,
    scoped_refptr<TrackedCallback> callback) {
  if (!server_name)
    return PP_ERROR_BADARGUMENT;

  if (state_.IsPending(TCPSocketState::SSL_CONNECT) ||
      TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(write_callback_)) {
    return PP_ERROR_INPROGRESS;
  }
  if (!state_.IsValidTransition(TCPSocketState::SSL_CONNECT))
    return PP_ERROR_FAILED;

  ssl_handshake_callback_ = callback;
  state_.SetPendingTransition(TCPSocketState::SSL_CONNECT);

  Call<PpapiPluginMsg_TCPSocket_SSLHandshakeReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_SSLHandshake(server_name,
                                          server_port,
                                          trusted_certificates_,
                                          untrusted_certificates_),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgSSLHandshakeReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource TCPSocketResourceBase::GetServerCertificateImpl() {
  if (!server_certificate_.get())
    return 0;
  return server_certificate_->GetReference();
}

PP_Bool TCPSocketResourceBase::AddChainBuildingCertificateImpl(
    PP_Resource certificate,
    PP_Bool trusted) {
  // TODO(raymes): This is exposed in the private PPB_TCPSocket_Private
  // interface for Flash but isn't currently implemented due to security
  // implications. It is exposed so that it can be hooked up on the Flash side
  // and if we decide to implement it we can do so without modifying the Flash
  // codebase.
  NOTIMPLEMENTED();
  return PP_FALSE;
}

int32_t TCPSocketResourceBase::ReadImpl(
    char* buffer,
    int32_t bytes_to_read,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || bytes_to_read <= 0)
    return PP_ERROR_BADARGUMENT;

  if (!state_.IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(read_callback_) ||
      state_.IsPending(TCPSocketState::SSL_CONNECT))
    return PP_ERROR_INPROGRESS;
  read_buffer_ = buffer;
  bytes_to_read_ =
      std::min(bytes_to_read,
               static_cast<int32_t>(TCPSocketResourceConstants::kMaxReadSize));
  read_callback_ = callback;

  Call<PpapiPluginMsg_TCPSocket_ReadReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Read(bytes_to_read_),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgReadReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::WriteImpl(
    const char* buffer,
    int32_t bytes_to_write,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || bytes_to_write <= 0)
    return PP_ERROR_BADARGUMENT;

  if (!state_.IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(write_callback_) ||
      state_.IsPending(TCPSocketState::SSL_CONNECT))
    return PP_ERROR_INPROGRESS;

  if (bytes_to_write > TCPSocketResourceConstants::kMaxWriteSize)
    bytes_to_write = TCPSocketResourceConstants::kMaxWriteSize;

  write_callback_ = callback;

  Call<PpapiPluginMsg_TCPSocket_WriteReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Write(std::string(buffer, bytes_to_write)),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgWriteReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::ListenImpl(
    int32_t backlog,
    scoped_refptr<TrackedCallback> callback) {
  if (backlog <= 0)
    return PP_ERROR_BADARGUMENT;
  if (state_.IsPending(TCPSocketState::LISTEN))
    return PP_ERROR_INPROGRESS;
  if (!state_.IsValidTransition(TCPSocketState::LISTEN))
    return PP_ERROR_FAILED;

  listen_callback_ = callback;
  state_.SetPendingTransition(TCPSocketState::LISTEN);

  Call<PpapiPluginMsg_TCPSocket_ListenReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Listen(backlog),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgListenReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::AcceptImpl(
    PP_Resource* accepted_tcp_socket,
    scoped_refptr<TrackedCallback> callback) {
  if (!accepted_tcp_socket)
    return PP_ERROR_BADARGUMENT;
  if (TrackedCallback::IsPending(accept_callback_))
    return PP_ERROR_INPROGRESS;
  if (state_.state() != TCPSocketState::LISTENING)
    return PP_ERROR_FAILED;

  accept_callback_ = callback;
  accepted_tcp_socket_ = accepted_tcp_socket;

  Call<PpapiPluginMsg_TCPSocket_AcceptReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Accept(),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgAcceptReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

void TCPSocketResourceBase::CloseImpl() {
  if (state_.state() == TCPSocketState::CLOSED)
    return;

  state_.DoTransition(TCPSocketState::CLOSE, true);

  Post(BROWSER, PpapiHostMsg_TCPSocket_Close());

  PostAbortIfNecessary(&bind_callback_);
  PostAbortIfNecessary(&connect_callback_);
  PostAbortIfNecessary(&ssl_handshake_callback_);
  PostAbortIfNecessary(&read_callback_);
  PostAbortIfNecessary(&write_callback_);
  PostAbortIfNecessary(&listen_callback_);
  PostAbortIfNecessary(&accept_callback_);
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  server_certificate_.reset();
  accepted_tcp_socket_ = NULL;
}

int32_t TCPSocketResourceBase::SetOptionImpl(
    PP_TCPSocket_Option name,
    const PP_Var& value,
    bool check_connect_state,
    scoped_refptr<TrackedCallback> callback) {
  SocketOptionData option_data;
  switch (name) {
    case PP_TCPSOCKET_OPTION_NO_DELAY: {
      if (check_connect_state && !state_.IsConnected())
        return PP_ERROR_FAILED;

      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      option_data.SetBool(PP_ToBool(value.value.as_bool));
      break;
    }
    case PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      if (check_connect_state && !state_.IsConnected())
        return PP_ERROR_FAILED;

      if (value.type != PP_VARTYPE_INT32)
        return PP_ERROR_BADARGUMENT;
      option_data.SetInt32(value.value.as_int);
      break;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }

  set_option_callbacks_.push(callback);

  Call<PpapiPluginMsg_TCPSocket_SetOptionReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_SetOption(name, option_data),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgSetOptionReply,
                 base::Unretained(this)),
      callback);
  return PP_OK_COMPLETIONPENDING;
}

void TCPSocketResourceBase::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

void TCPSocketResourceBase::OnPluginMsgBindReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& local_addr) {
  // It is possible that CloseImpl() has been called. We don't want to update
  // class members in this case.
  if (!state_.IsPending(TCPSocketState::BIND))
    return;

  DCHECK(TrackedCallback::IsPending(bind_callback_));
  if (params.result() == PP_OK) {
    local_addr_ = local_addr;
    state_.CompletePendingTransition(true);
  } else {
    state_.CompletePendingTransition(false);
  }
  RunCallback(bind_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgConnectReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  // It is possible that CloseImpl() has been called. We don't want to update
  // class members in this case.
  if (!state_.IsPending(TCPSocketState::CONNECT))
    return;

  DCHECK(TrackedCallback::IsPending(connect_callback_));
  if (params.result() == PP_OK) {
    local_addr_ = local_addr;
    remote_addr_ = remote_addr;
    state_.CompletePendingTransition(true);
  } else {
    if (version_ == TCP_SOCKET_VERSION_1_1_OR_ABOVE) {
      state_.CompletePendingTransition(false);
    } else {
      // In order to maintain backward compatibility, allow to connect the
      // socket again.
      state_ = TCPSocketState(TCPSocketState::INITIAL);
    }
  }
  RunCallback(connect_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgSSLHandshakeReply(
      const ResourceMessageReplyParams& params,
      const PPB_X509Certificate_Fields& certificate_fields) {
  // It is possible that CloseImpl() has been called. We don't want to
  // update class members in this case.
  if (!state_.IsPending(TCPSocketState::SSL_CONNECT))
    return;

  DCHECK(TrackedCallback::IsPending(ssl_handshake_callback_));
  if (params.result() == PP_OK) {
    state_.CompletePendingTransition(true);
    server_certificate_ = new PPB_X509Certificate_Private_Shared(
        OBJECT_IS_PROXY,
        pp_instance(),
        certificate_fields);
  } else {
    state_.CompletePendingTransition(false);
  }
  RunCallback(ssl_handshake_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgReadReply(
    const ResourceMessageReplyParams& params,
    const std::string& data) {
  // It is possible that CloseImpl() has been called. We shouldn't access the
  // buffer in that case. The user may have released it.
  if (!state_.IsConnected() || !TrackedCallback::IsPending(read_callback_) ||
      !read_buffer_) {
    return;
  }

  const bool succeeded = params.result() == PP_OK;
  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memmove(read_buffer_, data.c_str(), data.size());
  }
  read_buffer_ = NULL;
  bytes_to_read_ = -1;

  RunCallback(read_callback_,
              succeeded ? static_cast<int32_t>(data.size()) : params.result());
}

void TCPSocketResourceBase::OnPluginMsgWriteReply(
    const ResourceMessageReplyParams& params) {
  if (!state_.IsConnected() || !TrackedCallback::IsPending(write_callback_))
    return;
  RunCallback(write_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgListenReply(
    const ResourceMessageReplyParams& params) {
  if (!state_.IsPending(TCPSocketState::LISTEN))
    return;

  DCHECK(TrackedCallback::IsPending(listen_callback_));
  state_.CompletePendingTransition(params.result() == PP_OK);

  RunCallback(listen_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgAcceptReply(
    const ResourceMessageReplyParams& params,
    int pending_host_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  // It is possible that CloseImpl() has been called. We shouldn't access the
  // output parameter in that case. The user may have released it.
  if (state_.state() != TCPSocketState::LISTENING ||
      !TrackedCallback::IsPending(accept_callback_) || !accepted_tcp_socket_) {
    return;
  }

  if (params.result() == PP_OK) {
    *accepted_tcp_socket_ = CreateAcceptedSocket(pending_host_id, local_addr,
                                                 remote_addr);
  }
  accepted_tcp_socket_ = NULL;
  RunCallback(accept_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgSetOptionReply(
    const ResourceMessageReplyParams& params) {
  if (set_option_callbacks_.empty()) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TrackedCallback> callback = set_option_callbacks_.front();
  set_option_callbacks_.pop();
  if (TrackedCallback::IsPending(callback))
    RunCallback(callback, params.result());
}

void TCPSocketResourceBase::RunCallback(scoped_refptr<TrackedCallback> callback,
                                        int32_t pp_result) {
  callback->Run(ConvertNetworkAPIErrorForCompatibility(
      pp_result, version_ == TCP_SOCKET_VERSION_PRIVATE));
}

}  // namespace ppapi
}  // namespace proxy
