// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_RESOURCE_MESSAGE_HANDLER_H_
#define PPAPI_HOST_RESOURCE_MESSAGE_HANDLER_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/host/ppapi_host_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {

struct HostMessageContext;
struct ReplyMessageContext;

// This is the base class of classes that can handle resource messages. It
// mainly exists at present to share code for checking replies to resource
// messages are valid.
class PPAPI_HOST_EXPORT ResourceMessageHandler {
 public:
  ResourceMessageHandler();

  ResourceMessageHandler(const ResourceMessageHandler&) = delete;
  ResourceMessageHandler& operator=(const ResourceMessageHandler&) = delete;

  virtual ~ResourceMessageHandler();

  // Called when this handler should handle a particular message. This should
  // call into the the message handler implemented by subclasses (i.e.
  // |OnResourceMessageReceived|) and perform any additional work necessary to
  // handle the message (e.g. checking resource replies are valid). True is
  // returned if the message is handled and false otherwise.
  virtual bool HandleMessage(const IPC::Message& msg,
                             HostMessageContext* context) = 0;

  // Send a resource reply message.
  virtual void SendReply(const ReplyMessageContext& context,
                         const IPC::Message& msg) = 0;

 protected:
  // Runs the message handler and checks that a reply was sent if necessary.
  void RunMessageHandlerAndReply(const IPC::Message& msg,
                                 HostMessageContext* context);

  // Handles messages associated with a given resource object. If the flags
  // indicate that a response is required, the return value of this function
  // will be sent as a resource message "response" along with the message
  // specified in the reply of the context.
  //
  // You can do a response asynchronously by returning PP_OK_COMPLETIONPENDING.
  // This will cause the reply to be skipped, and the class implementing this
  // function will take responsibility for issuing the callback. The callback
  // can be issued inside OnResourceMessageReceived before it returns, or at
  // a future time.
  //
  // If you don't have a particular reply message, you can just ignore
  // the reply in the message context. However, if you have a reply more than
  // just the int32_t result code, set the reply to be the message of your
  // choosing.
  //
  // The default implementation just returns PP_ERROR_NOTSUPPORTED.
  virtual int32_t OnResourceMessageReceived(const IPC::Message& msg,
                                            HostMessageContext* context);
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_RESOURCE_MESSAGE_HANDLER_H_
