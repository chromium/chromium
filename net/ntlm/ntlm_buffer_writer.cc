// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ntlm/ntlm_buffer_writer.h"

#include <string.h>

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/containers/span_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace net::ntlm {

NtlmBufferWriter::NtlmBufferWriter(size_t buffer_len)
    : buffer_(buffer_len, 0) {}

NtlmBufferWriter::~NtlmBufferWriter() = default;

bool NtlmBufferWriter::CanWrite(size_t len) const {
  if (len == 0)
    return true;

  if (buffer_.empty()) {
    return false;
  }

  DCHECK_LE(GetCursor(), GetLength());

  return (len <= GetLength()) && (GetCursor() <= GetLength() - len);
}

bool NtlmBufferWriter::WriteUInt16(uint16_t value) {
  base::SpanWriter writer(base::span(buffer_).subspan(cursor_));
  if (writer.WriteU16LittleEndian(value)) {
    AdvanceCursor(sizeof(value));
    return true;
  }
  return false;
}

bool NtlmBufferWriter::WriteUInt32(uint32_t value) {
  base::SpanWriter writer(base::span(buffer_).subspan(cursor_));
  if (writer.WriteU32LittleEndian(value)) {
    AdvanceCursor(sizeof(value));
    return true;
  }
  return false;
}

bool NtlmBufferWriter::WriteUInt64(uint64_t value) {
  base::SpanWriter writer(base::span(buffer_).subspan(cursor_));
  if (writer.WriteU64LittleEndian(value)) {
    AdvanceCursor(sizeof(value));
    return true;
  }
  return false;
}

bool NtlmBufferWriter::WriteFlags(NegotiateFlags flags) {
  return WriteUInt32(static_cast<uint32_t>(flags));
}

bool NtlmBufferWriter::WriteBytes(base::span<const uint8_t> bytes) {
  if (bytes.size() == 0)
    return true;

  if (!CanWrite(bytes.size()))
    return false;

  GetSubspanAtCursor(bytes.size()).copy_from(bytes);
  AdvanceCursor(bytes.size());
  return true;
}

bool NtlmBufferWriter::WriteZeros(size_t count) {
  if (count == 0)
    return true;

  if (!CanWrite(count))
    return false;

  std::ranges::fill(GetSubspanAtCursor(count), 0);
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

  auto dest = GetSubspanAtCursor(num_bytes);
#if defined(ARCH_CPU_BIG_ENDIAN)

  for (int i = 0; i < num_bytes; i += 2) {
    dest[i] = str[i / 2] & 0xff;
    dest[i + 1] = str[i / 2] >> 8;
  }
#else
  dest.copy_from(base::as_byte_span(str));
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

void NtlmBufferWriter::SetCursor(size_t cursor) {
  DCHECK(!buffer_.empty() && cursor <= GetLength());

  cursor_ = cursor;
}

}  // namespace net::ntlm
