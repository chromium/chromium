// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm_buffer_writer.h"

#include <string.h>

#include <limits>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace net::ntlm {

NtlmBufferWriter::NtlmBufferWriter(size_t buffer_len)
    : buffer_(buffer_len, 0) {}

NtlmBufferWriter::~NtlmBufferWriter() = default;

bool NtlmBufferWriter::CanWrite(size_t len) const {
  if (len == 0)
    return true;

  if (!GetBufferPtr())
    return false;

  DCHECK_LE(GetCursor(), GetLength());

  return (len <= GetLength()) && (GetCursor() <= GetLength() - len);
}

bool NtlmBufferWriter::WriteUInt16(uint16_t value) {
  return WriteUInt<uint16_t>(value);
}

bool NtlmBufferWriter::WriteUInt32(uint32_t value) {
  return WriteUInt<uint32_t>(value);
}

bool NtlmBufferWriter::WriteUInt64(uint64_t value) {
  return WriteUInt<uint64_t>(value);
}

bool NtlmBufferWriter::WriteFlags(NegotiateFlags flags) {
  return WriteUInt32(static_cast<uint32_t>(flags));
}

bool NtlmBufferWriter::WriteBytes(base::span<const uint8_t> bytes) {
  if (bytes.size() == 0)
    return true;

  if (!CanWrite(bytes.size()))
    return false;

  memcpy(GetBufferPtrAtCursor(), bytes.data(), bytes.size());
  AdvanceCursor(bytes.size());
  return true;
}

bool NtlmBufferWriter::WriteZeros(size_t count) {
  if (count == 0)
    return true;

  if (!CanWrite(count))
    return false;

  memset(GetBufferPtrAtCursor(), 0, count);
  AdvanceCursor(count);
  return true;
}

bool NtlmBufferWriter::WriteSecurityBuffer(SecurityBuffer sec_buf) {
  return WriteUInt16(sec_buf.length) && WriteUInt16(sec_buf.length) &&
         WriteUInt32(sec_buf.offset);
}

bool NtlmBufferWriter::WriteAvPairHeader(TargetInfoAvId avid, uint16_t avlen) {
  if (!CanWrite(kAvPairHeaderLen))
    return false;

  bool result = WriteUInt16(static_cast<uint16_t>(avid)) && WriteUInt16(avlen);

  DCHECK(result);
  return result;
}

bool NtlmBufferWriter::WriteAvPairTerminator() {
  return WriteAvPairHeader(TargetInfoAvId::kEol, 0);
}

bool NtlmBufferWriter::WriteAvPair(const AvPair& pair) {
  if (!WriteAvPairHeader(pair))
    return false;

  if (pair.avid == TargetInfoAvId::kFlags) {
    if (pair.avlen != sizeof(uint32_t))
      return false;
    return WriteUInt32(static_cast<uint32_t>(pair.flags));
  } else {
    return WriteBytes(pair.buffer);
  }
}

bool NtlmBufferWriter::WriteUtf8String(const std::string& str) {
  return WriteBytes(base::as_byte_span(str));
}

bool NtlmBufferWriter::WriteUtf16AsUtf8String(const std::u16string& str) {
  std::string utf8 = base::UTF16ToUTF8(str);
  return WriteUtf8String(utf8);
}

bool NtlmBufferWriter::WriteUtf8AsUtf16String(const std::string& str) {
  std::u16string unicode = base::UTF8ToUTF16(str);
  return WriteUtf16String(unicode);
}

bool NtlmBufferWriter::WriteUtf16String(const std::u16string& str) {
  if (str.size() > std::numeric_limits<size_t>::max() / 2)
    return false;

  size_t num_bytes = str.size() * 2;
  if (num_bytes == 0)
    return true;

  if (!CanWrite(num_bytes))
    return false;

#if defined(ARCH_CPU_BIG_ENDIAN)
  uint8_t* ptr = reinterpret_cast<uint8_t*>(GetBufferPtrAtCursor());

  for (int i = 0; i < num_bytes; i += 2) {
    ptr[i] = str[i / 2] & 0xff;
    ptr[i + 1] = str[i / 2] >> 8;
  }
#else
  memcpy(reinterpret_cast<void*>(GetBufferPtrAtCursor()), str.c_str(),
         num_bytes);

#endif

  AdvanceCursor(num_bytes);
  return true;
}

bool NtlmBufferWriter::WriteSignature() {
  return WriteBytes(kSignature);
}

bool NtlmBufferWriter::WriteMessageType(MessageType message_type) {
  return WriteUInt32(static_cast<uint32_t>(message_type));
}

bool NtlmBufferWriter::WriteMessageHeader(MessageType message_type) {
  return WriteSignature() && WriteMessageType(message_type);
}

template <typename T>
bool NtlmBufferWriter::WriteUInt(T value) {
  size_t int_size = sizeof(T);
  if (!CanWrite(int_size))
    return false;

  for (size_t i = 0; i < int_size; i++) {
    GetBufferPtrAtCursor()[i] = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  }

  AdvanceCursor(int_size);
  return true;
}

void NtlmBufferWriter::SetCursor(size_t cursor) {
  DCHECK(GetBufferPtr() && cursor <= GetLength());

  cursor_ = cursor;
}

}  // namespace net::ntlm
