// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/proto_wrapper.h"

#include <limits>

#include "base/check_op.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace mojo_base {

ProtoWrapper::ProtoWrapper() = default;
ProtoWrapper::ProtoWrapper(mojo::DefaultConstruct::Tag passkey) {}
ProtoWrapper::~ProtoWrapper() = default;
ProtoWrapper::ProtoWrapper(ProtoWrapper&& other) = default;
ProtoWrapper& ProtoWrapper::operator=(ProtoWrapper&& other) = default;

ProtoWrapper::ProtoWrapper(const google::protobuf::MessageLite& message) {
  proto_name_ = message.GetTypeName();
  CHECK(message.ByteSizeLong() <= std::numeric_limits<int>::max());
  bytes_ = BigBuffer(message.ByteSizeLong());
  CHECK(message.SerializeToArray(bytes_->data(), bytes_->size()));
}

ProtoWrapper::ProtoWrapper(base::span<const uint8_t> data,
                           std::string type_name,
                           base::PassKey<ProtoWrapperBytes> passkey) {
  CHECK(!type_name.empty());
  CHECK_GT(data.size(), 0u);
  // Protobuf's unwrapping mechanisms take `int`.
  CHECK_LT(data.size(), static_cast<size_t>(std::numeric_limits<int>::max()));

  bytes_ = BigBuffer(data);
  proto_name_ = type_name;
}

bool ProtoWrapper::DeserializeToMessage(
    google::protobuf::MessageLite& message) const {
  if (!bytes_.has_value()) {
    return false;
  }
  // ProtoWrapper is either constructed from a message or from a mojom
  // typemapping, so must have a typename.
  if (message.GetTypeName() != proto_name_) {
    return false;
  }
  // ParseFromArray can only take `int`.
  if (bytes_->size() > std::numeric_limits<int>::max()) {
    return false;
  }

  if (bytes_->storage_type() == BigBuffer::StorageType::kBytes) {
    return message.ParseFromArray(bytes_->data(), bytes_->size());
  } else {
    // Make an in-process copy here as protobuf is not designed to
    // safely parse data that might be changing underneath it.
    auto as_span = base::make_span(bytes_->data(), bytes_->size());
    const std::vector<uint8_t> copy(as_span.begin(), as_span.end());
    return message.ParseFromArray(copy.data(), copy.size());
  }
}

}  // namespace mojo_base
