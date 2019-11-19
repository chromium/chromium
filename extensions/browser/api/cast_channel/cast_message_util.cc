// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_message_util.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "extensions/common/api/cast_channel.h"

namespace extensions {

bool MessageInfoToCastMessage(const api::cast_channel::MessageInfo& message,
                              ::cast_channel::CastMessage* message_proto) {
  DCHECK(message_proto);
  if (!message.data)
    return false;

  message_proto->set_protocol_version(
      ::cast_channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message_proto->set_source_id(message.source_id);
  message_proto->set_destination_id(message.destination_id);
  message_proto->set_namespace_(message.namespace_);
  // Determine the type of the base::Value and set the message payload
  // appropriately.
  switch (message.data->type()) {
    // JS string
    case base::Value::Type::STRING: {
      std::string data;
      if (message.data->GetAsString(&data)) {
        message_proto->set_payload_type(
            ::cast_channel::CastMessage_PayloadType_STRING);
        message_proto->set_payload_utf8(std::move(data));
      }
      break;
    }
    // JS ArrayBuffer
    case base::Value::Type::BINARY:
      message_proto->set_payload_type(
          ::cast_channel::CastMessage_PayloadType_BINARY);
      message_proto->set_payload_binary(message.data->GetBlob().data(),
                                        message.data->GetBlob().size());
      break;
    default:
      // Unknown value type.  message_proto will remain uninitialized because
      // payload_type is unset.
      break;
  }
  return message_proto->IsInitialized();
}

bool CastMessageToMessageInfo(const ::cast_channel::CastMessage& message_proto,
                              api::cast_channel::MessageInfo* message) {
  DCHECK(message);
  message->source_id = message_proto.source_id();
  message->destination_id = message_proto.destination_id();
  message->namespace_ = message_proto.namespace_();
  // Determine the type of the payload and fill base::Value appropriately.
  std::unique_ptr<base::Value> value;
  switch (message_proto.payload_type()) {
    case ::cast_channel::CastMessage_PayloadType_STRING:
      if (message_proto.has_payload_utf8())
        value.reset(new base::Value(message_proto.payload_utf8()));
      break;
    case ::cast_channel::CastMessage_PayloadType_BINARY:
      if (message_proto.has_payload_binary())
        value = base::Value::CreateWithCopiedBuffer(
            message_proto.payload_binary().data(),
            message_proto.payload_binary().size());
      break;
    default:
      // Unknown payload type. value will remain unset.
      break;
  }
  if (value.get()) {
    DCHECK(!message->data.get());
    message->data = std::move(value);
    return true;
  } else {
    return false;
  }
}

}  // namespace extensions
