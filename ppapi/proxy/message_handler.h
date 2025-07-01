// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MESSAGE_HANDLER_H_
#define PPAPI_PROXY_MESSAGE_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppp_message_handler.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace IPC {
class Message;
}

namespace ppapi {

class ScopedPPVar;

namespace proxy {

class MessageLoopResource;

// MessageHandler wraps a PPP_MessageHandler to encapsulate calling methods
// on the right thread and calling the Destroy function when this
// MessageHandler is destroyed.
class PPAPI_PROXY_EXPORT MessageHandler {
 public:
  // Create a MessageHandler. If any parameters are invalid, it will return a
  // null scoped_ptr and set |*error| appropriately.
  // |handler_if| is the struct of function pointers we will invoke. All of
  //              the function pointers within must be valid, or we fail
  //              with PP_ERROR_BADARGUMENT.
  // |user_data| is a pointer provided by the plugin that we pass back when we
  //             call functions in |handler_if|.
  // |message_loop| is the message loop where we will invoke functions in
  //                |handler_if|. Must not be the main thread message loop,
  //                to try to force the plugin to not over-subscribe the main
  //                thread. If it's the main thread loop, |error| will be set
  //                to PP_ERROR_WRONGTHREAD.
  // |error| is an out-param that will be set on failure.
  static std::unique_ptr<MessageHandler> Create(
      PP_Instance instance,
      const PPP_MessageHandler_0_2* handler_if,
      void* user_data,
      PP_Resource message_loop,
      int32_t* error);

  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;

  ~MessageHandler();

  bool LoopIsValid() const;

  void HandleMessage(ScopedPPVar var);
  void HandleBlockingMessage(ScopedPPVar var,
                             std::unique_ptr<IPC::Message> reply_msg);

 private:
  MessageHandler(PP_Instance instance,
                 const PPP_MessageHandler_0_2* handler_if,
                 void* user_data,
                 scoped_refptr<MessageLoopResource> message_loop);
  PP_Instance instance_;
  const PPP_MessageHandler_0_2* handler_if_;
  void* user_data_;
  scoped_refptr<MessageLoopResource> message_loop_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MESSAGE_HANDLER_H_
