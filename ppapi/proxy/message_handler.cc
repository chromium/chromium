// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/message_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_message.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {
namespace {

typedef void (*HandleMessageFunc)(PP_Instance, void*, const PP_Var*);
typedef void (*HandleBlockingMessageFunc)(
    PP_Instance, void*, const PP_Var*, PP_Var*);

void HandleMessageWrapper(HandleMessageFunc function,
                          PP_Instance instance,
                          void* user_data,
                          ScopedPPVar message_data) {
  CallWhileUnlocked(function, instance, user_data,
                    &message_data.get());
}

void HandleBlockingMessageWrapper(HandleBlockingMessageFunc function,
                                  PP_Instance instance,
                                  void* user_data,
                                  ScopedPPVar message_data,
                                  std::unique_ptr<IPC::Message> reply_msg) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  PP_Var result = PP_MakeUndefined();
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(true);
  CallWhileUnlocked(
      function, instance, user_data, &message_data.get(), &result);
  MessageLoopResource::GetCurrent()->
      set_currently_handling_blocking_message(false);
  PpapiMsg_PPPMessageHandler_HandleBlockingMessage::WriteReplyParams(
      reply_msg.get(),
      SerializedVarReturnValue::Convert(dispatcher, result),
      true /* was_handled */);
  dispatcher->Send(reply_msg.release());
}

}  // namespace

// static
std::unique_ptr<MessageHandler> MessageHandler::Create(
    PP_Instance instance,
    const PPP_MessageHandler_0_2* handler_if,
    void* user_data,
    PP_Resource message_loop,
    int32_t* error) {
  std::unique_ptr<MessageHandler> result;
  // The interface and all function pointers must be valid.
  if (!handler_if ||
      !handler_if->HandleMessage ||
      !handler_if->HandleBlockingMessage ||
      !handler_if->Destroy) {
    *error = PP_ERROR_BADARGUMENT;
    return result;
  }
  thunk::EnterResourceNoLock<thunk::PPB_MessageLoop_API>
      enter_loop(message_loop, true);
  if (enter_loop.failed()) {
    *error = PP_ERROR_BADRESOURCE;
    return result;
  }
  scoped_refptr<MessageLoopResource> message_loop_resource(
      static_cast<MessageLoopResource*>(enter_loop.object()));
  if (message_loop_resource->is_main_thread_loop()) {
    *error = PP_ERROR_WRONG_THREAD;
    return result;
  }

  result.reset(new MessageHandler(
      instance, handler_if, user_data, message_loop_resource));
  *error = PP_OK;
  return result;
}

MessageHandler::~MessageHandler() {
  // It's possible the message_loop_proxy is NULL if that loop has been quit.
  // In that case, we unfortunately just can't call Destroy.
  if (message_loop_->task_runner().get()) {
    // The posted task won't have the proxy lock, but that's OK, it doesn't
    // touch any internal state; it's a direct call on the plugin's function.
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(handler_if_->Destroy, instance_, user_data_));
  }
}

bool MessageHandler::LoopIsValid() const {
  return !!message_loop_->task_runner().get();
}

void MessageHandler::HandleMessage(ScopedPPVar var) {
  message_loop_->task_runner()->PostTask(
      FROM_HERE, RunWhileLocked(base::BindOnce(&HandleMessageWrapper,
                                               handler_if_->HandleMessage,
                                               instance_, user_data_, var)));
}

void MessageHandler::HandleBlockingMessage(
    ScopedPPVar var,
    std::unique_ptr<IPC::Message> reply_msg) {
  message_loop_->task_runner()->PostTask(
      FROM_HERE,
      RunWhileLocked(base::BindOnce(
          &HandleBlockingMessageWrapper, handler_if_->HandleBlockingMessage,
          instance_, user_data_, var, std::move(reply_msg))));
}

MessageHandler::MessageHandler(
    PP_Instance instance,
    const PPP_MessageHandler_0_2* handler_if,
    void* user_data,
    scoped_refptr<MessageLoopResource> message_loop)
    : instance_(instance),
      handler_if_(handler_if),
      user_data_(user_data),
      message_loop_(message_loop) {
}

}  // namespace proxy
}  // namespace ppapi
