// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_
#define PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_

#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "ppapi/c/pp_stdint.h"

namespace ppapi {
namespace proxy {

class ResourceMessageCallParams;
class ResourceMessageReplyParams;
class SerializedHandle;

// Extends IPC::TestSink to add extra capabilities for searching for and
// decoding resource messages.
class ResourceMessageTestSink : public IPC::TestSink {
 public:
  ResourceMessageTestSink();
  ~ResourceMessageTestSink() override;

  // IPC::TestSink.
  // Overridden to handle sync messages.
  bool Send(IPC::Message* msg) override;

  // Sets the reply message that will be returned to the next sync message sent.
  // This test sink owns any reply messages passed into this method.
  void SetSyncReplyMessage(IPC::Message* reply_msg);

  // Searches the queue for the first resource call message with a nested
  // message matching the given ID. On success, returns true and populates the
  // given params and nested message.
  bool GetFirstResourceCallMatching(uint32_t id,
                                    ResourceMessageCallParams* params,
                                    IPC::Message* nested_msg) const;

  // Like GetFirstResourceCallMatching except for replies.
  bool GetFirstResourceReplyMatching(uint32_t id,
                                     ResourceMessageReplyParams* params,
                                     IPC::Message* nested_msg);

  // Searches the queue for all resource call messages with a nested message
  // matching the given ID.
  typedef std::pair<ResourceMessageCallParams, IPC::Message> ResourceCall;
  typedef std::vector<ResourceCall> ResourceCallVector;
  ResourceCallVector GetAllResourceCallsMatching(uint32_t id);

  // Like GetAllResourceCallsMatching except for replies.
  typedef std::pair<ResourceMessageReplyParams, IPC::Message> ResourceReply;
  typedef std::vector<ResourceReply> ResourceReplyVector;
  ResourceReplyVector GetAllResourceRepliesMatching(uint32_t id);

 private:
  std::unique_ptr<IPC::Message> sync_reply_msg_;
};

// This is a message handler which generates reply messages for synchronous
// resource calls. This allows unit testing of the plugin side of resources
// which send sync messages. If you want to reply to a sync message type named
// |PpapiHostMsg_X_Y| with |PpapiPluginMsg_X_YReply| then usage would be as
// follows (from within |PluginProxyTest|s):
//
// PpapiHostMsg_X_YReply my_reply;
// ResourceSyncCallHandler handler(&sink(),
//                                 PpapiHostMsg_X_Y::ID,
//                                 PP_OK,
//                                 my_reply);
// sink().AddFilter(&handler);
// // Do stuff to send a sync message ...
// // You can check handler.last_handled_msg() to ensure the correct message was
// // handled.
// sink().RemoveFilter(&handler);
class ResourceSyncCallHandler : public IPC::Listener {
 public:
  ResourceSyncCallHandler(ResourceMessageTestSink* test_sink,
                          uint32_t incoming_type,
                          int32_t result,
                          const IPC::Message& reply_msg);
  ~ResourceSyncCallHandler() override;

  // IPC::Listener.
  bool OnMessageReceived(const IPC::Message& message) override;

  IPC::Message last_handled_msg() { return last_handled_msg_; }

  // Sets a handle to be appended to the ReplyParams.
  void set_serialized_handle(
      std::unique_ptr<SerializedHandle> serialized_handle);

 private:
  ResourceMessageTestSink* test_sink_;
  uint32_t incoming_type_;
  int32_t result_;
  std::unique_ptr<SerializedHandle> serialized_handle_;
  IPC::Message reply_msg_;
  IPC::Message last_handled_msg_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_RESOURCE_MESSAGE_TEST_SINK_H_
