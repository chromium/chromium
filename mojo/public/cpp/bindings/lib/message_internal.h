// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_

#include <stdint.h>

#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {

class Message;

namespace internal {

template <typename T>
class Array_Data;

#pragma pack(push, 1)

struct MessageHeader : internal::StructHeader {
  // Interface ID for identifying multiple interfaces running on the same
  // message pipe.
  uint32_t interface_id;
  // Message name, which is scoped to the interface that the message belongs to.
  uint32_t name;
  // A combination of zero or more of the flag constants defined within the
  // Message class.
  uint32_t flags;
  // A unique (hopefully) value for a message. Used in tracing, forming the
  // lower part of the 64-bit trace id, which is used to match trace events for
  // sending and receiving a message (`name` forms the upper part).
  uint32_t trace_nonce;
};
static_assert(sizeof(MessageHeader) == 24, "Bad sizeof(MessageHeader)");

struct MessageHeaderV1 : MessageHeader {
  // Only used if either kFlagExpectsResponse or kFlagIsResponse is set in
  // order to match responses with corresponding requests.
  uint64_t request_id;
};
static_assert(sizeof(MessageHeaderV1) == 32, "Bad sizeof(MessageHeaderV1)");

struct MessageHeaderV2 : MessageHeaderV1 {
  MessageHeaderV2();
  GenericPointer payload;
  Pointer<Array_Data<uint32_t>> payload_interface_ids;
};
static_assert(sizeof(MessageHeaderV2) == 48, "Bad sizeof(MessageHeaderV2)");

struct MessageHeaderV3 : MessageHeaderV2 {
  MessageHeaderV3();
  int64_t creation_timeticks_us;
};
static_assert(sizeof(MessageHeaderV3) == 56, "Bad sizeof(MessageHeaderV3)");

#pragma pack(pop)

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageDispatchContext {
 public:
  explicit MessageDispatchContext(Message* message);

  MessageDispatchContext(const MessageDispatchContext&) = delete;
  MessageDispatchContext& operator=(const MessageDispatchContext&) = delete;

  ~MessageDispatchContext();

  static MessageDispatchContext* current();

  base::OnceCallback<void(std::string_view)> GetBadMessageCallback();

 private:
  raw_ptr<MessageDispatchContext> outer_context_;
  raw_ptr<Message> message_;
};

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
size_t ComputeSerializedMessageSize(uint32_t flags,
                                    size_t payload_size,
                                    size_t payload_interface_id_count,
                                    int64_t creation_timeticks_us);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
size_t EstimateSerializedMessageSize(uint32_t message_name,
                                     size_t payload_size,
                                     size_t total_size,
                                     size_t estimated_payload_size);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
