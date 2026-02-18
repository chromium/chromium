// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/message.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace extensions {

namespace {

// Performs a deep copy of a `blink::CloneableMessage`.
//
// This is necessary because `blink::CloneableMessage` contains move-only
// resources like Mojo pipes (e.g., for Blobs). A simple copy would invalidate
// the original message's handles. This function clones those handles to ensure
// both the original and the clone remain functional.
blink::CloneableMessage CloneCloneableMessage(
    const blink::CloneableMessage& message) {
  // ShallowClone() clones the Mojo handles (blobs, tokens) but shares the
  // encoded_message buffer.
  blink::CloneableMessage clone = message.ShallowClone();
  // Ensure the clone has its own copy of the encoded_message buffer.
  clone.EnsureDataIsOwned();
  return clone;
}

}  // namespace

Message::Message() = default;

Message::Message(MessageData data,
                 bool user_gesture,
                 bool from_privileged_context)
    : data_(std::move(data)),
      user_gesture_(user_gesture),
      from_privileged_context_(from_privileged_context) {
  if (std::holds_alternative<StructuredCloneMessageData>(data_)) {
    CHECK(base::FeatureList::IsEnabled(
        extensions_features::kStructuredCloningForMessaging));
  }
}

Message::Message(Message&& other) = default;

Message::~Message() = default;

Message& Message::operator=(Message&& other) = default;

bool Message::EqualsForTesting(const Message& other) const {
  if (format() != other.format() || user_gesture_ != other.user_gesture_) {
    return false;
  }

  switch (format()) {
    case mojom::SerializationFormat::kJson:
      return std::get<std::string>(data_) == other.data();
    case mojom::SerializationFormat::kStructuredClone: {
      CHECK(base::FeatureList::IsEnabled(
          extensions_features::kStructuredCloningForMessaging));
      const auto& this_msg = std::get<StructuredCloneMessageData>(data_);
      const auto& other_msg = other.structured_message();
      // For JS `Blobs`: this `encoded_message` comparison checks that the JS
      // objects are structurally identical (e.g. both are { file: Blob }), but
      // it does not compare the underlying `Blob` data byte-by-byte.
      if (this_msg.encoded_message != other_msg.encoded_message ||
          this_msg.blobs.size() != other_msg.blobs.size()) {
        return false;
      }
      // Check that each blob appears to point to the same underlying data.
      for (size_t i = 0; i < this_msg.blobs.size(); ++i) {
        // `blink::mojom::SerializedBlob` does not have an equals operator so we
        // manually compare the metadata (uuid, content_type, size) which
        // defines Blob identity.
        const auto& this_msg_blob = *this_msg.blobs[i];
        const auto& other_msg_blob = *other_msg.blobs[i];

        if (this_msg_blob.uuid != other_msg_blob.uuid ||
            this_msg_blob.content_type != other_msg_blob.content_type ||
            this_msg_blob.size != other_msg_blob.size) {
          return false;
        }
      }
      return true;
    }
    default:
      // We only support JSON or structured cloning serialization formats.
      NOTREACHED();
  }
}

Message Message::Clone() const {
  MessageData data;
  if (std::holds_alternative<std::string>(data_)) {
    data = std::get<std::string>(data_);
  } else {
    data = CloneCloneableMessage(std::get<StructuredCloneMessageData>(data_));
  }

  return Message(std::move(data), user_gesture_, from_privileged_context_);
}

StructuredCloneMessageData Message::TakeStructuredMessage() {
  CHECK(std::holds_alternative<StructuredCloneMessageData>(data_));
  return std::move(std::get<StructuredCloneMessageData>(data_));
}

const std::string& Message::data() const {
  CHECK(std::holds_alternative<std::string>(data_));
  return std::get<std::string>(data_);
}

const StructuredCloneMessageData& Message::structured_message() const {
  CHECK(std::holds_alternative<StructuredCloneMessageData>(data_));
  return std::get<StructuredCloneMessageData>(data_);
}

}  // namespace extensions
