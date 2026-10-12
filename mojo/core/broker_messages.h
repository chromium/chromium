// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_MESSAGES_H_
#define MOJO_CORE_BROKER_MESSAGES_H_

#include "base/compiler_specific.h"
#include "mojo/core/channel.h"

namespace mojo {
namespace core {

#pragma pack(push, 1)

enum BrokerMessageType : uint32_t {
  BUFFER_REQUEST = 1,
  BUFFER_RESPONSE = 2,
};

struct BrokerMessageHeader {
  BrokerMessageType type;
  uint32_t padding;
};

static_assert(IsAlignedForChannelMessage(sizeof(BrokerMessageHeader)),
              "Invalid header size.");

struct BufferRequestData {
  uint32_t size;
};

struct BufferResponseData {
  uint64_t guid_high;
  uint64_t guid_low;
};

#pragma pack(pop)

template <typename T>
inline bool GetBrokerMessageData(Channel::Message* message, T** out_data) {
  const size_t required_size = sizeof(BrokerMessageHeader) + sizeof(T);
  if (message->payload_size() < required_size) {
    return false;
  }

  auto* header = static_cast<BrokerMessageHeader*>(message->mutable_payload());
  *out_data = reinterpret_cast<T*>(UNSAFE_TODO(header + 1));
  return true;
}

template <typename T>
inline Channel::MessagePtr CreateBrokerMessage(BrokerMessageType type,
                                               size_t num_handles,
                                               T** out_message_data) {
  const size_t message_size =
      sizeof(BrokerMessageHeader) + sizeof(**out_message_data);
  Channel::MessagePtr message =
      Channel::Message::CreateMessage(message_size, num_handles);
  BrokerMessageHeader* header =
      reinterpret_cast<BrokerMessageHeader*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_message_data = reinterpret_cast<T*>(UNSAFE_TODO(header + 1));
  return message;
}

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_MESSAGES_H_
