// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef MOJO_CORE_BROKER_MESSAGES_H_
#define MOJO_CORE_BROKER_MESSAGES_H_

#include "build/build_config.h"
#include "mojo/core/channel.h"

namespace mojo {
namespace core {

#pragma pack(push, 1)

enum BrokerMessageType : uint32_t {
  INIT,
  BUFFER_REQUEST,
  BUFFER_RESPONSE,
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

#if BUILDFLAG(IS_WIN)
struct InitData {
  // NOTE: InitData in the payload is followed by string16 data with exactly
  // |pipe_name_length| wide characters (i.e., |pipe_name_length|*2 bytes.)
  // This applies to Windows only.
  uint32_t pipe_name_length;
};
#endif

#pragma pack(pop)

template <typename T>
inline bool GetBrokerMessageData(Channel::Message* message, T** out_data) {
  const size_t required_size = sizeof(BrokerMessageHeader) + sizeof(T);
  if (message->payload_size() < required_size)
    return false;

  auto* header = static_cast<BrokerMessageHeader*>(message->mutable_payload());
  *out_data = reinterpret_cast<T*>(header + 1);
  return true;
}

template <typename T>
inline Channel::MessagePtr CreateBrokerMessage(
    BrokerMessageType type,
    size_t num_handles,
    size_t extra_data_size,
    T** out_message_data,
    void** out_extra_data = nullptr) {
  const size_t message_size = sizeof(BrokerMessageHeader) +
                              sizeof(**out_message_data) + extra_data_size;
  Channel::MessagePtr message =
      Channel::Message::CreateMessage(message_size, num_handles);
  BrokerMessageHeader* header =
      reinterpret_cast<BrokerMessageHeader*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_message_data = reinterpret_cast<T*>(header + 1);
  if (out_extra_data)
    *out_extra_data = *out_message_data + 1;
  return message;
}

inline Channel::MessagePtr CreateBrokerMessage(
    BrokerMessageType type,
    size_t num_handles,
    std::nullptr_t** dummy_out_data) {
  Channel::MessagePtr message =
      Channel::Message::CreateMessage(sizeof(BrokerMessageHeader), num_handles);
  BrokerMessageHeader* header =
      reinterpret_cast<BrokerMessageHeader*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  return message;
}

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_MESSAGES_H_
