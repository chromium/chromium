// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace mojo_base {
class BigBuffer;
}

namespace blink {

// A interface represents ArrayBuffer payload type in WebMessage.
class BLINK_COMMON_EXPORT WebMessageArrayBufferPayload {
 public:
  virtual ~WebMessageArrayBufferPayload() = default;

  // Returns the length of the payload.
  virtual size_t GetLength() const = 0;

  // Convert the underlying buffer to a span if possible. Or return empty if
  // can't (like Java ByteArray). JNI API does not provide a way to get a
  // pointer to the underlying array memory, so another API |CopyInto| should be
  // used instead to avoid an extra copy.
  virtual absl::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const = 0;

  // Copy the underlying buffer into the given destination. The |dest| must be
  // larger than or equal to the payload. This method is always available
  // regarding various backing stores.
  virtual void CopyInto(base::span<uint8_t> dest) const = 0;

  // Create a new WebMessageArrayBufferPayload from BigBuffer.
  static std::unique_ptr<WebMessageArrayBufferPayload> CreateFromBigBuffer(
      mojo_base::BigBuffer buffer);

  // Create a new WebMessageArrayBufferPayload from vector for testing.
  static std::unique_ptr<WebMessageArrayBufferPayload> CreateForTesting(
      std::vector<uint8_t> data);
};

// Represent WebMessage payload type between browser and renderer process.
using WebMessagePayload =
    absl::variant<std::u16string,
                  std::unique_ptr<WebMessageArrayBufferPayload>>;

// To support exposing HTML message ports to Java, it is necessary to be able
// to encode and decode message data using the same serialization format as V8.
// That format is an implementation detail of V8, but we cannot invoke V8 in
// the browser process. Rather than IPC over to the renderer process to execute
// the V8 serialization code, we duplicate some of the serialization logic
// (just for simple string or array buffer messages) here. This is
// a trade-off between overall complexity / performance and code duplication.
// Fortunately, we only need to handle string messages and this serialization
// format is static, as it is a format we currently persist to disk via
// IndexedDB.

BLINK_COMMON_EXPORT TransferableMessage
EncodeWebMessagePayload(const WebMessagePayload& payload);

BLINK_COMMON_EXPORT absl::optional<WebMessagePayload> DecodeToWebMessagePayload(
    TransferableMessage message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
