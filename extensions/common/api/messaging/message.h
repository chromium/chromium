// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_

#include <string>
#include <variant>

#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace extensions {

// TODO(crbug.com/40321352): `mojo_base::BigBuffer`, by itself, doesn't support
// JS `Blob`s because it doesn't have the `Blob`'s metadata. Switch to
// `blink::mojom::CloneableMessage` to get `Blob` support.
using StructuredCloneMessageWireData = mojo_base::BigBuffer;
// C++ type-safe representation of the `extensions::mojom::MessageData` `union`.
using MessageData = std::variant<std::string, StructuredCloneMessageWireData>;

// Represents a message sent between extension components, encapsulating the
// data payload and associated metadata. This class is represented in mojom as
// the `extensions::mojom::Message` struct in `message_port.mojom`.
//
// Data Payload: A `Message` can hold one of two types of data, distinguished by
// the `format()` field:
//
// 1. JSON-serialized data: For backward compatibility and simple messages, the
//    payload can be a JSON string stored in the `data_` member. The `format()`
//    will be `mojom::SerializationFormat::kJson`.
//
// 2. Structure-cloned data: For complex, non-JSON-serializable objects, this
//    class can hold data serialized by Blink's `(Web)SerializedScriptValue`.
//    This wire data is stored in the `structured_data_` member as a
//    `mojo_base::BigBuffer`, and the `format()` will be
//    `mojom::SerializationFormat::kStructuredClone`.
//
// Metadata:
// - `user_gesture`: This boolean indicates whether the message was sent as a
//   direct result of a user action (e.g., a button click). This is important
//   for determining if an action can be performed without requiring a temporary
//   user activation.
//
// - `from_privileged_context`: This boolean indicates whether the message
//   originated from a trusted, extension-specific context (like a background
//   script) rather than a potentially untrusted context (like a content
//   script).
class Message {
 public:
  Message();
  Message(MessageData data,
          mojom::SerializationFormat format,
          bool user_gesture,
          bool from_privileged_context = false);
  Message(const Message& other);
  Message(Message&& other);
  ~Message();

  Message& operator=(const Message& other);
  Message& operator=(Message&& other);

  bool operator==(const Message& other) const;

  // TODO(crbug.com/40321352): Merge `data()` and `structured_data()` into
  // `message_data()` once the feature is complete and callers are updated to
  // handle the variant.
  const std::string& data() const;
  const StructuredCloneMessageWireData& structured_data() const;
  const MessageData& message_data() const { return data_; }
  mojom::SerializationFormat format() const { return format_; }
  bool user_gesture() const { return user_gesture_; }
  bool from_privileged_context() const { return from_privileged_context_; }

 private:
  MessageData data_;
  // TODO(crbug.com/40321352): Convert `format_` to an unknown value since we
  // shouldn't assume JSON by default anymore.
  mojom::SerializationFormat format_ = mojom::SerializationFormat::kJson;
  bool user_gesture_ = false;
  // The equality check skips `from_privileged_context` because this field is
  // used only for histograms.
  bool from_privileged_context_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
