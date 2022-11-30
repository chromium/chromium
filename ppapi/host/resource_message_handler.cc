// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/resource_message_handler.h"

#include "base/logging.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"

namespace ppapi {
namespace host {

ResourceMessageHandler::ResourceMessageHandler() {
}

ResourceMessageHandler::~ResourceMessageHandler() {
}

void ResourceMessageHandler::RunMessageHandlerAndReply(
    const IPC::Message& msg,
    HostMessageContext* context) {
  ReplyMessageContext reply_context = context->MakeReplyMessageContext();
  // CAUTION: Handling the message may cause the destruction of this object.
  // The message handler should ensure that if there is a chance that the
  // object will be destroyed, PP_OK_COMPLETIONPENDING is returned as the
  // result of the message handler. Otherwise the code below will attempt to
  // send a reply message on a destroyed object.
  reply_context.params.set_result(OnResourceMessageReceived(msg, context));

  // Sanity check the resource handler. Note if the result was
  // "completion pending" the resource host may have already sent the reply.
  if (reply_context.params.result() == PP_OK_COMPLETIONPENDING) {
    // Message handler should have only returned a pending result if a
    // response will be sent to the plugin.
    DCHECK(context->params.has_callback());

    // Message handler should not have written a message to be returned if
    // completion is pending.
    DCHECK(context->reply_msg.type() == 0);
  } else if (!context->params.has_callback()) {
    // When no response is required, the message handler should not have
    // written a message to be returned.
    DCHECK(context->reply_msg.type() == 0);

    // If there is no callback and the result of running the message handler
    // was not PP_OK the client won't find out.
    DLOG_IF(WARNING, reply_context.params.result() != PP_OK)
        << "'Post' message handler failed to complete successfully.";
  }

  if (context->params.has_callback() &&
      reply_context.params.result() != PP_OK_COMPLETIONPENDING)
    SendReply(reply_context, context->reply_msg);
}

int32_t ResourceMessageHandler::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  return PP_ERROR_NOTSUPPORTED;
}

}  // namespace host
}  // namespace ppapi
