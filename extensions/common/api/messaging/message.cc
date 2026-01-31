// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/message.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "extensions/common/extension_features.h"

namespace extensions {

Message::Message() = default;

Message::Message(MessageData data,
                 mojom::SerializationFormat format,
                 bool user_gesture,
                 bool from_privileged_context)
    : data_(std::move(data)),
      format_(format),
      user_gesture_(user_gesture),
      from_privileged_context_(from_privileged_context) {
  if (format_ == mojom::SerializationFormat::kJson) {
    CHECK(std::holds_alternative<std::string>(data_));
  } else if (format_ == mojom::SerializationFormat::kStructuredClone) {
    CHECK(std::holds_alternative<StructuredCloneMessageWireData>(data_));
    CHECK(base::FeatureList::IsEnabled(
        extensions_features::kStructuredCloningForMessaging));
  }
}

Message::Message(const Message& other)
    : format_(other.format_),
      user_gesture_(other.user_gesture_),
      from_privileged_context_(other.from_privileged_context_) {
  if (std::holds_alternative<std::string>(other.data_)) {
    data_ = std::get<std::string>(other.data_);
  } else {
    data_ = std::get<StructuredCloneMessageWireData>(other.data_).Clone();
  }
}

Message::Message(Message&& other) = default;

Message::~Message() = default;

Message& Message::operator=(const Message& other) {
  format_ = other.format_;
  user_gesture_ = other.user_gesture_;
  from_privileged_context_ = other.from_privileged_context_;
  if (std::holds_alternative<std::string>(other.data_)) {
    data_ = std::get<std::string>(other.data_);
  } else {
    data_ = std::get<StructuredCloneMessageWireData>(other.data_).Clone();
  }
  return *this;
}

Message& Message::operator=(Message&& other) = default;

bool Message::operator==(const Message& other) const {
  if (format_ != other.format()) {
    return false;
  }

  bool message_data_equal = false;
  switch (format_) {
    case mojom::SerializationFormat::kJson:
      message_data_equal = std::get<std::string>(data_) == other.data();
      break;
    case mojom::SerializationFormat::kStructuredClone:
      CHECK(base::FeatureList::IsEnabled(
          extensions_features::kStructuredCloningForMessaging));
      // `mojo_base::BigBuffer` does not have a comparison operator so we
      // convert to comparable `base::span`s.
      message_data_equal =
          base::span<const uint8_t>(std::get<StructuredCloneMessageWireData>(
              data_)) == base::span<const uint8_t>(other.structured_data());
      break;
    default:
      // We only support JSON or structured cloning serialization formats.
      NOTREACHED();
  }

  return message_data_equal && user_gesture_ == other.user_gesture_;
}

const std::string& Message::data() const {
  CHECK(std::holds_alternative<std::string>(data_));
  return std::get<std::string>(data_);
}

const StructuredCloneMessageWireData& Message::structured_data() const {
  CHECK(std::holds_alternative<StructuredCloneMessageWireData>(data_));
  return std::get<StructuredCloneMessageWireData>(data_);
}

}  // namespace extensions
