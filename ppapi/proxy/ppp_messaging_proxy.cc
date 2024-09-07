// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_messaging_proxy.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/message_handler.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {
namespace proxy {

namespace {

MessageHandler* GetMessageHandler(Dispatcher* dispatcher,
                                  PP_Instance instance) {
  if (!dispatcher || !dispatcher->IsPlugin()) {
    NOTREACHED();
  }
  PluginDispatcher* plugin_dispatcher =
      static_cast<PluginDispatcher*>(dispatcher);
  InstanceData* instance_data = plugin_dispatcher->GetInstanceData(instance);
  if (!instance_data)
    return NULL;

  return instance_data->message_handler.get();
}

void ResetMessageHandler(Dispatcher* dispatcher, PP_Instance instance) {
  if (!dispatcher || !dispatcher->IsPlugin()) {
    NOTREACHED();
  }
  PluginDispatcher* plugin_dispatcher =
      static_cast<PluginDispatcher*>(dispatcher);
  InstanceData* instance_data = plugin_dispatcher->GetInstanceData(instance);
  if (!instance_data)
    return;

  instance_data->message_handler.reset();
}

}  // namespace

PPP_Messaging_Proxy::PPP_Messaging_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_messaging_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_messaging_impl_ = static_cast<const PPP_Messaging*>(
        dispatcher->local_get_interface()(PPP_MESSAGING_INTERFACE));
  }
}

PPP_Messaging_Proxy::~PPP_Messaging_Proxy() {
}

bool PPP_Messaging_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Messaging_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPMessaging_HandleMessage,
                        OnMsgHandleMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        PpapiMsg_PPPMessageHandler_HandleBlockingMessage,
        OnMsgHandleBlockingMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Messaging_Proxy::OnMsgHandleMessage(
    PP_Instance instance, SerializedVarReceiveInput message_data) {
  PP_Var received_var(message_data.GetForInstance(dispatcher(), instance));
  MessageHandler* message_handler = GetMessageHandler(dispatcher(), instance);
  if (message_handler) {
    if (message_handler->LoopIsValid()) {
      message_handler->HandleMessage(ScopedPPVar(received_var));
      return;
    } else {
      // If the MessageHandler's loop has been quit, then we should treat it as
      // though it has been unregistered and start sending messages to the
      // default handler. This might mean the plugin has lost messages, but
      // there's not really anything sane we can do about it. They should have
      // used UnregisterMessageHandler.
      ResetMessageHandler(dispatcher(), instance);
    }
  }
  // If we reach this point, then there's no message handler registered, so
  // we send to the default PPP_Messaging one for the instance.

  // SerializedVarReceiveInput will decrement the reference count, but we want
  // to give the recipient a reference in the legacy API.
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(received_var);
  CallWhileUnlocked(ppp_messaging_impl_->HandleMessage,
                    instance,
                    received_var);
}

void PPP_Messaging_Proxy::OnMsgHandleBlockingMessage(
    PP_Instance instance,
    SerializedVarReceiveInput message_data,
    IPC::Message* reply_msg) {
  ScopedPPVar received_var(message_data.GetForInstance(dispatcher(), instance));
  MessageHandler* message_handler = GetMessageHandler(dispatcher(), instance);
  if (message_handler) {
    if (message_handler->LoopIsValid()) {
      message_handler->HandleBlockingMessage(received_var,
                                             base::WrapUnique(reply_msg));
      return;
    } else {
      // If the MessageHandler's loop has been quit, then we should treat it as
      // though it has been unregistered. Also see the note for PostMessage.
      ResetMessageHandler(dispatcher(), instance);
    }
  }
  // We have no handler, but we still need to respond to unblock the renderer
  // and inform the JavaScript caller.
  PpapiMsg_PPPMessageHandler_HandleBlockingMessage::WriteReplyParams(
      reply_msg,
      SerializedVarReturnValue::Convert(dispatcher(), PP_MakeUndefined()),
      false /* was_handled */);
  dispatcher()->Send(reply_msg);
}


}  // namespace proxy
}  // namespace ppapi
