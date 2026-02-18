// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_

#include <string>
#include <utility>
#include <variant>

#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"

namespace extensions {

using StructuredCloneMessageData = blink::CloneableMessage;
// C++ type-safe representation of the `extensions::mojom::MessageData` `union`.
using MessageData = std::variant<std::string, StructuredCloneMessageData>;

// Represents a message sent between extension components, encapsulating the
// data payload and associated metadata. This class is represented in mojom as
// the `extensions::mojom::Message` struct in `message_port.mojom`.
//
// Data Payload: A `Message` can hold one of two types of data:
//
// 1. JSON-serialized data: For backward compatibility and simple messages, the
//    payload can be a JSON string stored in the `data_` member. The `format()`
//    will be `mojom::SerializationFormat::kJson`.
//
// 2. Structure-cloned data: For complex, non-JSON-serializable objects, this
//    class can hold data serialized by Blink's `(Web)SerializedScriptValue`.
//    This wire data is stored in the `data_` member as a
//    `blink::CloneableMessage`, and the `format()` will be
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
          bool user_gesture,
          bool from_privileged_context = false);
  // This class is move-only to:
  // 1) discourage unintentional copies (messages can be quite large see
  //    `extensions::mojom::kMaxMessageBytes`).
  // 2) satisfy `blink::CloneableMessage` (via `StructuredCloneMessageData`)
  //    which contains move-only resources like
  //    `mojo::PendingRemote<blink::mojom::Blob>`.
  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;

  Message(Message&& other);
  ~Message();

  Message& operator=(Message&& other);

  // ALL messaging data cannot be easily compared equally (and due to it's
  // potential size is not encouraged anyways). A specifically bad example are
  // JS `Blob`s that use mojom remotes for access to the underlying message
  // bytes. Pulling the bytes of both objects over mojom to do the comparison is
  // not efficient.
  bool operator==(const Message& other) const = delete;

  // Equality tester for messages.
  //
  // NOTE: For messages containing Blobs, this only compares Blob metadata
  // (UUID, content type, and size) to determine identity, rather than
  // performing a byte-for-byte comparison of the Blob's content.
  bool EqualsForTesting(const Message& other) const;

  // Performs a deep copy of the message.
  //
  // Note: messages can be large so this should only be called when necessary,
  // otherwise use `std::move` mechanics to avoid unnecessary copies. However,
  // in some cases (e.g. `ExtensionMessagePort::DispatchOnMessage`), a single
  // message needs to be broadcast to multiple listeners/contexts. This method
  // provides an explicit way to copy the message for such scenarios.
  Message Clone() const;

  // Moves the underlying `StructuredCloneMessageData` out of this `Message` and
  // returns it.
  //
  // WHY: This is necessary to pass the internal move-only message data (which
  // includes Mojo handles for Blobs) to Blink APIs (like
  // `WebSerializedScriptValue`) or other components that expect the
  // `blink::CloneableMessage` type directly rather than the `Message` wrapper.
  //
  // NOTE: This leaves this `Message` object without its underlying message
  // data. Attempting to access the data after this call will result in a
  // `CHECK` failure.
  StructuredCloneMessageData TakeStructuredMessage();

  // TODO(crbug.com/40321352): Merge `data()` and `structured_message()` into
  // `message_data()` once the feature is complete and callers are updated to
  // handle the variant.
  const std::string& data() const;
  const StructuredCloneMessageData& structured_message() const;
  const MessageData& message_data() const { return data_; }
  MessageData& message_data() { return data_; }

  mojom::SerializationFormat format() const {
    return std::holds_alternative<std::string>(data_)
               ? mojom::SerializationFormat::kJson
               : mojom::SerializationFormat::kStructuredClone;
  }
  bool user_gesture() const { return user_gesture_; }
  bool from_privileged_context() const { return from_privileged_context_; }

 private:
  MessageData data_;
  bool user_gesture_ = false;
  // The equality check skips `from_privileged_context` because this field is
  // used only for histograms.
  bool from_privileged_context_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
