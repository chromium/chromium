// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_message_test_sink.h"

#include <stddef.h>

#include <tuple>

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

namespace {

// Backend for GetAllResource[Calls|Replies]Matching.
template <class WrapperMessage, class Params>
std::vector<std::pair<Params, IPC::Message>> GetAllResourceMessagesMatching(
    const ResourceMessageTestSink& sink,
    uint32_t id) {
  std::vector<std::pair<Params, IPC::Message> > result;
  for (size_t i = 0; i < sink.message_count(); i++) {
    const IPC::Message* msg = sink.GetMessageAt(i);
    if (msg->type() == WrapperMessage::ID) {
      typename WrapperMessage::Param params;
      WrapperMessage::Read(msg, &params);
      Params cur_params = std::get<0>(params);
      IPC::Message cur_msg = std::get<1>(params);
      if (cur_msg.type() == id) {
        result.push_back(std::make_pair(cur_params, cur_msg));
      }
    }
  }
  return result;
}

}  // namespace

ResourceMessageTestSink::ResourceMessageTestSink() {
}

ResourceMessageTestSink::~ResourceMessageTestSink() {
}

bool ResourceMessageTestSink::Send(IPC::Message* msg) {
  int message_id = 0;
  std::unique_ptr<IPC::MessageReplyDeserializer> reply_deserializer;
  if (msg->is_sync()) {
    reply_deserializer =
        static_cast<IPC::SyncMessage*>(msg)->TakeReplyDeserializer();
    message_id = IPC::SyncMessage::GetMessageId(*msg);
  }
  bool result = IPC::TestSink::Send(msg);  // Deletes |msg|.
  if (sync_reply_msg_.get()) {
    // |sync_reply_msg_| should always be a reply to the pending sync message.
    DCHECK(IPC::SyncMessage::IsMessageReplyTo(*sync_reply_msg_.get(),
                                              message_id));
    reply_deserializer->SerializeOutputParameters(*sync_reply_msg_.get());
    sync_reply_msg_.reset(NULL);
  }
  return result;
}

void ResourceMessageTestSink::SetSyncReplyMessage(IPC::Message* reply_msg) {
  DCHECK(!sync_reply_msg_.get());
  sync_reply_msg_.reset(reply_msg);
}

bool ResourceMessageTestSink::GetFirstResourceCallMatching(
    uint32_t id,
    ResourceMessageCallParams* params,
    IPC::Message* nested_msg) const {
  ResourceCallVector matching_messages =
      GetAllResourceMessagesMatching<PpapiHostMsg_ResourceCall,
                                     ResourceMessageCallParams>(*this, id);
  if (matching_messages.empty())
    return false;

  *params = matching_messages[0].first;
  *nested_msg = matching_messages[0].second;
  return true;
}

bool ResourceMessageTestSink::GetFirstResourceReplyMatching(
    uint32_t id,
    ResourceMessageReplyParams* params,
    IPC::Message* nested_msg) {
  ResourceReplyVector matching_messages =
      GetAllResourceMessagesMatching<PpapiPluginMsg_ResourceReply,
                                     ResourceMessageReplyParams>(*this, id);
  if (matching_messages.empty())
    return false;

  *params = matching_messages[0].first;
  *nested_msg = matching_messages[0].second;
  return true;
}

ResourceMessageTestSink::ResourceCallVector
ResourceMessageTestSink::GetAllResourceCallsMatching(uint32_t id) {
  return GetAllResourceMessagesMatching<PpapiHostMsg_ResourceCall,
                                        ResourceMessageCallParams>(*this, id);
}

ResourceMessageTestSink::ResourceReplyVector
ResourceMessageTestSink::GetAllResourceRepliesMatching(uint32_t id) {
  return GetAllResourceMessagesMatching<PpapiPluginMsg_ResourceReply,
                                        ResourceMessageReplyParams>(*this, id);
}

ResourceSyncCallHandler::ResourceSyncCallHandler(
    ResourceMessageTestSink* test_sink,
    uint32_t incoming_type,
    int32_t result,
    const IPC::Message& reply_msg)
    : test_sink_(test_sink),
      incoming_type_(incoming_type),
      result_(result),
      reply_msg_(reply_msg) {}

ResourceSyncCallHandler::~ResourceSyncCallHandler() {
}

bool ResourceSyncCallHandler::OnMessageReceived(const IPC::Message& msg) {
  if (msg.type() != PpapiHostMsg_ResourceSyncCall::ID)
    return false;
  PpapiHostMsg_ResourceSyncCall::Schema::SendParam send_params;
  bool success = PpapiHostMsg_ResourceSyncCall::ReadSendParam(
      &msg, &send_params);
  DCHECK(success);
  ResourceMessageCallParams call_params = std::get<0>(send_params);
  IPC::Message call_msg = std::get<1>(send_params);
  if (call_msg.type() != incoming_type_)
    return false;
  IPC::Message* wrapper_reply_msg = IPC::SyncMessage::GenerateReply(&msg);
  ResourceMessageReplyParams reply_params(call_params.pp_resource(),
                                          call_params.sequence());
  reply_params.set_result(result_);
  if (serialized_handle_)
    reply_params.AppendHandle(std::move(*serialized_handle_));
  PpapiHostMsg_ResourceSyncCall::WriteReplyParams(
      wrapper_reply_msg, reply_params, reply_msg_);
  test_sink_->SetSyncReplyMessage(wrapper_reply_msg);

  // Stash a copy of the message for inspection later.
  last_handled_msg_ = call_msg;
  return true;
}

void ResourceSyncCallHandler::set_serialized_handle(
    std::unique_ptr<SerializedHandle> serialized_handle) {
  serialized_handle_ = std::move(serialized_handle);
}

}  // namespace proxy
}  // namespace ppapi
