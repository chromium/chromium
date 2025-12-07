// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/message.h"

namespace extensions {

Message::Message() = default;

Message::Message(const std::string& data,
                 mojom::SerializationFormat format,
                 bool user_gesture,
                 bool from_privileged_context)
    : data_(data),
      format_(format),
      user_gesture_(user_gesture),
      from_privileged_context_(from_privileged_context) {}

Message::Message(const Message& other) = default;

Message::Message(Message&& other) = default;

Message::~Message() = default;

Message& Message::operator=(const Message& other) = default;

Message& Message::operator=(Message&& other) = default;

bool Message::operator==(const Message& other) const {
  // Skipping the equality check for `from_privileged_context` here
  // because this field is used only for histograms.
  return data_ == other.data_ && user_gesture_ == other.user_gesture_ &&
         format_ == other.format_;
}

}  // namespace extensions
