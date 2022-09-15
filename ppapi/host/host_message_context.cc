// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/host_message_context.h"

namespace ppapi {
namespace host {

ReplyMessageContext::ReplyMessageContext()
    : sync_reply_msg(NULL), routing_id(MSG_ROUTING_NONE) {}

ReplyMessageContext::ReplyMessageContext(
    const ppapi::proxy::ResourceMessageReplyParams& cp,
    IPC::Message* sync_reply_msg,
    int routing_id)
    : params(cp),
      sync_reply_msg(sync_reply_msg),
      routing_id(routing_id) {
}

ReplyMessageContext::~ReplyMessageContext() {
}

HostMessageContext::HostMessageContext(
    const ppapi::proxy::ResourceMessageCallParams& cp)
    : params(cp),
      sync_reply_msg(NULL),
      routing_id(MSG_ROUTING_NONE) {
}

HostMessageContext::HostMessageContext(
    int routing_id,
    const ppapi::proxy::ResourceMessageCallParams& cp)
    : params(cp),
      sync_reply_msg(NULL),
      routing_id(routing_id) {
}

HostMessageContext::HostMessageContext(
    const ppapi::proxy::ResourceMessageCallParams& cp,
    IPC::Message* reply_msg)
    : params(cp),
      sync_reply_msg(reply_msg),
      routing_id(MSG_ROUTING_NONE) {
}

HostMessageContext::~HostMessageContext() {
}

ReplyMessageContext HostMessageContext::MakeReplyMessageContext() const {
  ppapi::proxy::ResourceMessageReplyParams reply_params(params.pp_resource(),
                                                        params.sequence());
  return ReplyMessageContext(reply_params, sync_reply_msg, routing_id);
}

}  // namespace host
}  // namespace ppapi
