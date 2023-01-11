// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_server_socket_private_resource.h"

#include "base/functional/bind.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/tcp_socket_private_resource.h"

namespace ppapi {
namespace proxy {

TCPServerSocketPrivateResource::TCPServerSocketPrivateResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance),
      state_(STATE_BEFORE_LISTENING),
      local_addr_() {
  SendCreate(BROWSER, PpapiHostMsg_TCPServerSocket_CreatePrivate());
}

TCPServerSocketPrivateResource::~TCPServerSocketPrivateResource() {
}

thunk::PPB_TCPServerSocket_Private_API*
TCPServerSocketPrivateResource::AsPPB_TCPServerSocket_Private_API() {
  return this;
}

int32_t TCPServerSocketPrivateResource::Listen(
    const PP_NetAddress_Private* addr,
    int32_t backlog,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (state_ != STATE_BEFORE_LISTENING)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(listen_callback_))
    return PP_ERROR_INPROGRESS;

  listen_callback_ = callback;

  // Send the request, the browser will call us back via ListenACK
  Call<PpapiPluginMsg_TCPServerSocket_ListenReply>(
      BROWSER, PpapiHostMsg_TCPServerSocket_Listen(*addr, backlog),
      base::BindOnce(&TCPServerSocketPrivateResource::OnPluginMsgListenReply,
                     base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPServerSocketPrivateResource::Accept(
    PP_Resource* tcp_socket,
    scoped_refptr<TrackedCallback> callback) {
  if (!tcp_socket)
    return PP_ERROR_BADARGUMENT;
  if (state_ != STATE_LISTENING)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(accept_callback_))
    return PP_ERROR_INPROGRESS;

  accept_callback_ = callback;

  Call<PpapiPluginMsg_TCPServerSocket_AcceptReply>(
      BROWSER, PpapiHostMsg_TCPServerSocket_Accept(),
      base::BindOnce(&TCPServerSocketPrivateResource::OnPluginMsgAcceptReply,
                     base::Unretained(this), tcp_socket));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPServerSocketPrivateResource::GetLocalAddress(
    PP_NetAddress_Private* addr) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (state_ != STATE_LISTENING)
    return PP_ERROR_FAILED;
  *addr = local_addr_;
  return PP_OK;
}

void TCPServerSocketPrivateResource::StopListening() {
  if (state_ == STATE_CLOSED)
    return;
  state_ = STATE_CLOSED;
  Post(BROWSER, PpapiHostMsg_TCPServerSocket_StopListening());
  if (TrackedCallback::IsPending(listen_callback_))
    listen_callback_->PostAbort();
  if (TrackedCallback::IsPending(accept_callback_))
    accept_callback_->PostAbort();
}

void TCPServerSocketPrivateResource::OnPluginMsgListenReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& local_addr) {
  if (state_ != STATE_BEFORE_LISTENING ||
      !TrackedCallback::IsPending(listen_callback_)) {
    return;
  }
  if (params.result() == PP_OK) {
    local_addr_ = local_addr;
    state_ = STATE_LISTENING;
  }
  listen_callback_->Run(params.result());
}

void TCPServerSocketPrivateResource::OnPluginMsgAcceptReply(
    PP_Resource* tcp_socket,
    const ResourceMessageReplyParams& params,
    int pending_resource_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  DCHECK(tcp_socket);
  if (state_ != STATE_LISTENING ||
      !TrackedCallback::IsPending(accept_callback_)) {
    return;
  }
  if (params.result() == PP_OK) {
    *tcp_socket = (new TCPSocketPrivateResource(connection(), pp_instance(),
                                                pending_resource_id,
                                                local_addr,
                                                remote_addr))->GetReference();
  }
  accept_callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
