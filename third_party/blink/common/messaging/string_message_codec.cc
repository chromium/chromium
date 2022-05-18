// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <vector>

#include "base/containers/buffer_iterator.h"
#include "base/logging.h"

namespace blink {
namespace {

const uint32_t kVarIntShift = 7;
const uint32_t kVarIntMask = (1 << kVarIntShift) - 1;

const uint8_t kVersionTag = 0xFF;
const uint8_t kPaddingTag = '\0';
const uint8_t kOneByteStringTag = '"';
const uint8_t kTwoByteStringTag = 'c';

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

std::vector<uint8_t> EncodeStringMessage(const std::u16string& data) {
  std::vector<uint8_t> buffer;
  WriteUint8(kVersionTag, &buffer);
  WriteUint32(kVersion, &buffer);

  if (ContainsOnlyLatin1(data)) {
    std::string data_latin1(data.begin(), data.end());
    WriteUint8(kOneByteStringTag, &buffer);
    WriteUint32(data_latin1.size(), &buffer);
    WriteBytes(data_latin1.c_str(), data_latin1.size(), &buffer);
  } else {
    size_t num_bytes = data.size() * sizeof(char16_t);
    if ((buffer.size() + 1 + BytesNeededForUint32(num_bytes)) & 1)
      WriteUint8(kPaddingTag, &buffer);
    WriteUint8(kTwoByteStringTag, &buffer);
    WriteUint32(num_bytes, &buffer);
    WriteBytes(reinterpret_cast<const char*>(data.data()), num_bytes, &buffer);
  }

  return buffer;
}

bool DecodeStringMessage(base::span<const uint8_t> encoded_data,
                         std::u16string* result) {
  base::BufferIterator<const uint8_t> iter(encoded_data);
  uint8_t tag;

  // Discard any leading version and padding tags.
  // There may be more than one version, due to Blink and V8 having separate
  // version tags.
  do {
    if (!ReadUint8(iter, &tag))
      return false;
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version))
      return false;
  } while (tag == kVersionTag || tag == kPaddingTag);

  switch (tag) {
    case kOneByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return false;
      auto span = iter.Span<char>(num_bytes / sizeof(char));
      result->assign(span.begin(), span.end());
      return span.size_bytes() == num_bytes;
    }
    case kTwoByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return false;
      auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
      result->assign(span.begin(), span.end());
      return span.size_bytes() == num_bytes;
    }
  }

  DLOG(WARNING) << "Unexpected tag: " << tag;
  return false;
}

}  // namespace blink
