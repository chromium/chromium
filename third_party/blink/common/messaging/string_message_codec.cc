// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

namespace blink {
namespace {

// An ArrayBufferPayload impl based on std::vector.
class VectorArrayBuffer : public WebMessageArrayBufferPayload {
 public:
  VectorArrayBuffer(std::vector<uint8_t> data, size_t position, size_t length)
      : data_(std::move(data)), position_(position), length_(length) {
    size_t size = base::CheckAdd(position_, length_).ValueOrDie();
    CHECK_GE(data_.size(), size);
  }

  size_t GetLength() const override { return length_; }

  bool GetIsResizableByUserJavaScript() const override {
    // VectorArrayBuffers are not used for ArrayBuffer transfers and are
    // currently always fixed-length. Structured cloning resizables ArrayBuffers
    // is not yet supported in SMC.
    return false;
  }

  size_t GetMaxByteLength() const override { return length_; }

  std::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return AsSpan();
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    dest.copy_from(AsSpan());
  }

 private:
  base::span<const uint8_t> AsSpan() const {
    return base::span(data_).subspan(position_, length_);
  }

  std::vector<uint8_t> data_;
  size_t position_;
  size_t length_;
};

// An ArrayBufferPayload impl based on mojo::BigBuffer.
class BigBufferArrayBuffer : public WebMessageArrayBufferPayload {
 public:
  explicit BigBufferArrayBuffer(mojo_base::BigBuffer data,
                                std::optional<size_t> max_byte_length)
      : data_(std::move(data)), max_byte_length_(max_byte_length) {
    DCHECK(!max_byte_length || *max_byte_length >= GetLength());
  }

  size_t GetLength() const override { return data_.size(); }

  bool GetIsResizableByUserJavaScript() const override {
    return max_byte_length_.has_value();
  }

  size_t GetMaxByteLength() const override {
    return max_byte_length_.value_or(GetLength());
  }

  std::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return base::make_span(data_);
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    dest.copy_from(base::make_span(data_));
  }

 private:
  mojo_base::BigBuffer data_;
  std::optional<size_t> max_byte_length_;
};

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

void WriteBytes(base::span<const uint8_t> bytes, std::vector<uint8_t>* buffer) {
  buffer->insert(buffer->end(), bytes.begin(), bytes.end());
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

// static
std::unique_ptr<WebMessageArrayBufferPayload>
WebMessageArrayBufferPayload::CreateFromBigBuffer(
    mojo_base::BigBuffer buffer,
    std::optional<size_t> max_byte_length) {
  return std::make_unique<BigBufferArrayBuffer>(std::move(buffer),
                                                max_byte_length);
}

// static
std::unique_ptr<WebMessageArrayBufferPayload>
WebMessageArrayBufferPayload::CreateForTesting(std::vector<uint8_t> data) {
  auto size = data.size();
  return std::make_unique<VectorArrayBuffer>(std::move(data), 0, size);
}

TransferableMessage EncodeWebMessagePayload(const WebMessagePayload& payload) {
  TransferableMessage message;
  std::vector<uint8_t> buffer;
  WriteUint8(kVersionTag, &buffer);
  WriteUint32(kVersion, &buffer);
  absl::visit(
      base::Overloaded{
          [&](const std::u16string& str) {
            if (ContainsOnlyLatin1(str)) {
              std::string data_latin1(str.cbegin(), str.cend());
              WriteUint8(kOneByteStringTag, &buffer);
              WriteUint32(data_latin1.size(), &buffer);
              WriteBytes(base::as_byte_span(data_latin1), &buffer);
            } else {
              auto str_as_bytes = base::as_byte_span(str);
              if ((buffer.size() + 1 +
                   BytesNeededForUint32(str_as_bytes.size())) &
                  1) {
                WriteUint8(kPaddingTag, &buffer);
              }
              WriteUint8(kTwoByteStringTag, &buffer);
              WriteUint32(str_as_bytes.size(), &buffer);
              WriteBytes(str_as_bytes, &buffer);
            }
          },
          [&](const std::unique_ptr<WebMessageArrayBufferPayload>&
                  array_buffer) {
            WriteUint8(kArrayBufferTransferTag, &buffer);
            // Write at the first slot.
            WriteUint32(0, &buffer);

            mojo_base::BigBuffer big_buffer(array_buffer->GetLength());
            array_buffer->CopyInto(base::make_span(big_buffer));
            message.array_buffer_contents_array.push_back(
                mojom::SerializedArrayBufferContents::New(
                    std::move(big_buffer),
                    array_buffer->GetIsResizableByUserJavaScript(),
                    array_buffer->GetMaxByteLength()));
          }},
      payload);

  message.owned_encoded_message = std::move(buffer);
  message.encoded_message = message.owned_encoded_message;

  return message;
}

std::optional<WebMessagePayload> DecodeToWebMessagePayload(
    TransferableMessage message) {
  base::BufferIterator<const uint8_t> iter(message.encoded_message);
  uint8_t tag;

  // Discard the outer envelope, including trailer info if applicable.
  if (!ReadUint8(iter, &tag))
    return std::nullopt;
  if (tag == kVersionTag) {
    uint32_t version = 0;
    if (!ReadUint32(iter, &version))
      return std::nullopt;
    static constexpr uint32_t kMinWireFormatVersionWithTrailer = 21;
    if (version >= kMinWireFormatVersionWithTrailer) {
      // In these versions, we expect kTrailerOffsetTag (0xFE) followed by an
      // offset and size. See details in
      // third_party/blink/renderer/core/v8/serialization/serialization_tag.h.
      auto span = iter.Span<uint8_t>(1 + sizeof(uint64_t) + sizeof(uint32_t));
      if (span.empty() || span[0] != 0xFE)
        return std::nullopt;
    }
    if (!ReadUint8(iter, &tag))
      return std::nullopt;
  }

  // Discard any leading version and padding tags.
  while (tag == kVersionTag || tag == kPaddingTag) {
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version))
      return std::nullopt;
    if (!ReadUint8(iter, &tag))
      return std::nullopt;
  }

  switch (tag) {
    case kOneByteStringTag: {
      // Use of unsigned char rather than char here matters, so that Latin-1
      // characters are zero-extended rather than sign-extended
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return std::nullopt;
      auto span = iter.Span<unsigned char>(num_bytes / sizeof(unsigned char));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? std::make_optional(WebMessagePayload(std::move(str)))
                 : std::nullopt;
    }
    case kTwoByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return std::nullopt;
      auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? std::make_optional(WebMessagePayload(std::move(str)))
                 : std::nullopt;
    }
    case kArrayBuffer: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return std::nullopt;
      size_t position = iter.position();
      return position + num_bytes == iter.total_size()
                 ? std::make_optional(
                       WebMessagePayload(std::make_unique<VectorArrayBuffer>(
                           std::move(message.owned_encoded_message), position,
                           num_bytes)))
                 : std::nullopt;
    }
    case kArrayBufferTransferTag: {
      uint32_t array_buffer_index;
      if (!ReadUint32(iter, &array_buffer_index))
        return std::nullopt;
      // We only support transfer ArrayBuffer at the first index.
      if (array_buffer_index != 0)
        return std::nullopt;
      if (message.array_buffer_contents_array.size() != 1)
        return std::nullopt;
      auto& array_buffer_contents = message.array_buffer_contents_array[0];
      std::optional<size_t> max_byte_length;
      if (array_buffer_contents->is_resizable_by_user_javascript) {
        max_byte_length.emplace(array_buffer_contents->max_byte_length);
      }
      return std::make_optional(
          WebMessagePayload(std::make_unique<BigBufferArrayBuffer>(
              std::move(array_buffer_contents->contents), max_byte_length)));
    }
  }

  DLOG(WARNING) << "Unexpected tag: " << tag;
  return std::nullopt;
}

}  // namespace blink
