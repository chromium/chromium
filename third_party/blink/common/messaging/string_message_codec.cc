// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <vector>

#include "base/containers/buffer_iterator.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

namespace blink {
namespace {

// Template helpers for visiting std::variant.
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

const uint32_t kVarIntShift = 7;
const uint32_t kVarIntMask = (1 << kVarIntShift) - 1;

const uint8_t kVersionTag = 0xFF;
const uint8_t kPaddingTag = '\0';
// serialization_tag, see v8/src/objects/value-serializer.cc
const uint8_t kOneByteStringTag = '"';
const uint8_t kTwoByteStringTag = 'c';
const uint8_t kArrayBuffer = 'B';
const uint8_t kArrayBufferTransferTag = 't';

const uint32_t kVersion = 10;

static size_t BytesNeededForUint32(uint32_t value) {
  size_t result = 0;
  do {
    result++;
    value >>= kVarIntShift;
  } while (value);
  return result;
}

void WriteUint8(uint8_t value, std::vector<uint8_t>* buffer) {
  buffer->push_back(value);
}

void WriteUint32(uint32_t value, std::vector<uint8_t>* buffer) {
  for (;;) {
    uint8_t b = (value & kVarIntMask);
    value >>= kVarIntShift;
    if (!value) {
      WriteUint8(b, buffer);
      break;
    }
    WriteUint8(b | (1 << kVarIntShift), buffer);
  }
}

void WriteBytes(const char* bytes,
                size_t num_bytes,
                std::vector<uint8_t>* buffer) {
  buffer->insert(buffer->end(), bytes, bytes + num_bytes);
}

bool ReadUint8(base::BufferIterator<const uint8_t>& iter, uint8_t* value) {
  if (const uint8_t* ptr = iter.Object<uint8_t>()) {
    *value = *ptr;
    return true;
  }
  return false;
}

bool ReadUint32(base::BufferIterator<const uint8_t>& iter, uint32_t* value) {
  *value = 0;
  uint8_t current_byte;
  int shift = 0;
  do {
    if (!ReadUint8(iter, &current_byte))
      return false;

    *value |= (static_cast<uint32_t>(current_byte & kVarIntMask) << shift);
    shift += kVarIntShift;
  } while (current_byte & (1 << kVarIntShift));
  return true;
}

bool ContainsOnlyLatin1(const std::u16string& data) {
  char16_t x = 0;
  for (char16_t c : data)
    x |= c;
  return !(x & 0xFF00);
}

}  // namespace

TransferableMessage EncodeWebMessagePayload(const WebMessagePayload& payload) {
  TransferableMessage message;
  std::vector<uint8_t> buffer;
  WriteUint8(kVersionTag, &buffer);
  WriteUint32(kVersion, &buffer);

  absl::visit(
      overloaded{
          [&](const std::u16string& str) {
            if (ContainsOnlyLatin1(str)) {
              std::string data_latin1(str.cbegin(), str.cend());
              WriteUint8(kOneByteStringTag, &buffer);
              WriteUint32(data_latin1.size(), &buffer);
              WriteBytes(data_latin1.c_str(), data_latin1.size(), &buffer);
            } else {
              size_t num_bytes = str.size() * sizeof(char16_t);
              if ((buffer.size() + 1 + BytesNeededForUint32(num_bytes)) & 1)
                WriteUint8(kPaddingTag, &buffer);
              WriteUint8(kTwoByteStringTag, &buffer);
              WriteUint32(num_bytes, &buffer);
              WriteBytes(reinterpret_cast<const char*>(str.data()), num_bytes,
                         &buffer);
            }
          },
          [&](const std::vector<uint8_t>& array_buffer) {
            WriteUint8(kArrayBufferTransferTag, &buffer);
            // Write at the first slot.
            WriteUint32(0, &buffer);

            mojo_base::BigBuffer big_buffer(array_buffer);
            message.array_buffer_contents_array.push_back(
                mojom::SerializedArrayBufferContents::New(
                    std::move(big_buffer)));
          }},
      payload);

  message.owned_encoded_message = std::move(buffer);
  message.encoded_message = message.owned_encoded_message;

  return message;
}

absl::optional<WebMessagePayload> DecodeToWebMessagePayload(
    const TransferableMessage& message) {
  base::BufferIterator<const uint8_t> iter(message.encoded_message);
  uint8_t tag;

  // Discard the outer envelope, including trailer info if applicable.
  if (!ReadUint8(iter, &tag))
    return absl::nullopt;
  if (tag == kVersionTag) {
    uint32_t version = 0;
    if (!ReadUint32(iter, &version))
      return absl::nullopt;
    static constexpr uint32_t kMinWireFormatVersionWithTrailer = 21;
    if (version >= kMinWireFormatVersionWithTrailer) {
      // In these versions, we expect kTrailerOffsetTag (0xFE) followed by an
      // offset and size. See details in
      // third_party/blink/renderer/core/v8/serialization/serialization_tag.h.
      auto span = iter.Span<uint8_t>(1 + sizeof(uint64_t) + sizeof(uint32_t));
      if (span.empty() || span[0] != 0xFE)
        return absl::nullopt;
    }
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  // Discard any leading version and padding tags.
  while (tag == kVersionTag || tag == kPaddingTag) {
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version))
      return absl::nullopt;
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  switch (tag) {
    case kOneByteStringTag: {
      // Use of unsigned char rather than char here matters, so that Latin-1
      // characters are zero-extended rather than sign-extended
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<unsigned char>(num_bytes / sizeof(unsigned char));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(WebMessagePayload(std::move(str)))
                 : absl::nullopt;
    }
    case kTwoByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(WebMessagePayload(std::move(str)))
                 : absl::nullopt;
    }
    case kArrayBuffer: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<uint8_t>(num_bytes);
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(
                       WebMessagePayload(std::vector(span.begin(), span.end())))
                 : absl::nullopt;
    }
    case kArrayBufferTransferTag: {
      uint32_t array_buffer_index;
      if (!ReadUint32(iter, &array_buffer_index))
        return absl::nullopt;
      // We only support transfer ArrayBuffer at the first index.
      if (array_buffer_index != 0)
        return absl::nullopt;
      if (message.array_buffer_contents_array.size() != 1)
        return absl::nullopt;
      const auto& big_buffer = message.array_buffer_contents_array[0]->contents;
      // Data is from renderer process, copy it first before use.
      return std::vector(big_buffer.data(),
                         big_buffer.data() + big_buffer.size());
    }
  }

  DLOG(WARNING) << "Unexpected tag: " << tag;
  return absl::nullopt;
}

}  // namespace blink
