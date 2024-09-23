// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_H_
#define MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace mojo_base {

namespace mojom {
class ProtoWrapperDataView;
}  // namespace mojom

template <typename T>
concept IsProtoMessage =
    !std::is_abstract<T>() &&
    std::is_base_of<google::protobuf::MessageLite, T>::value;

class COMPONENT_EXPORT(MOJO_BASE_PROTOBUF_SUPPORT) ProtoWrapper {
 public:
  ProtoWrapper(ProtoWrapper&& other);
  ProtoWrapper& operator=(ProtoWrapper&& other);
  ProtoWrapper(const ProtoWrapper&) = delete;
  ProtoWrapper& operator=(const ProtoWrapper&) = delete;
  ~ProtoWrapper();

  // Exposed so mojo can create this class.
  explicit ProtoWrapper(mojo::DefaultConstruct::Tag passkey);

  // Construct from a protobuf message. May CHECK if the message fails
  // serialization. Once constructed the message can be discarded. The typename
  // of the message is stored along with the bytes that serialize the message,
  // and must match the typename of the message this wrapper is deserialized to.
  explicit ProtoWrapper(const google::protobuf::MessageLite& message);

  // Construct from an already serialized proto stream - only use this if the
  // stream has come from an external source (e.g. the network, an OS service)
  // and you need to get the message into the mojo IPC system with a typename.
  // This constructor does not validate that the stream of bytes can populate
  // its wrapped protobuf Message until an unwrapping is attempted.
  // Makes a copy of the data in the provided span.
  explicit ProtoWrapper(base::span<const uint8_t> data,
                        std::string type_name,
                        base::PassKey<ProtoWrapperBytes> passkey);

  template <IsProtoMessage ProtoMessage>
  std::optional<ProtoMessage> As() const {
    ProtoMessage message;
    if (DeserializeToMessage(message)) {
      return message;
    }
    return std::nullopt;
  }

  // Access this to store the bytes somewhere else, or pass to another IPC
  // system. The bytes may be mapped from a hostile process so while the size
  // cannot change, the contents might. If you want to unpack the contained
  // protobuf Message, use As<T>();
  std::optional<base::span<const uint8_t>> byte_span(
      base::PassKey<ProtoWrapperBytes> passkey) const {
    if (!is_valid()) {
      return std::nullopt;
    }
    return base::span(*bytes_);
  }

 private:
  friend struct mojo::StructTraits<mojo_base::mojom::ProtoWrapperDataView,
                                   mojo_base::ProtoWrapper>;
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, TraitsOk);
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, LargeMessage);
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, TraitsEquivilentMessages);
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, TraitsDistinctMessages);
  FRIEND_TEST_ALL_PREFIXES(ProtoWrapperTest, ToFromBytes);

  // Prevent creation of invalid wrappers.
  ProtoWrapper();

  // is_valid() implies the wrapper wraps some data - it does not mean that the
  // bytes will deserialize to a valid message.
  bool is_valid() const { return bytes_.has_value(); }

  bool DeserializeToMessage(google::protobuf::MessageLite& message) const;

  std::string proto_name_;
  std::optional<BigBuffer> bytes_;
};

}  // namespace mojo_base

#endif  // MOJO_PUBLIC_CPP_BASE_PROTO_WRAPPER_H_
