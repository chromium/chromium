// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm_buffer_reader.h"

#include <string.h>

#include "base/check_op.h"

namespace net::ntlm {

NtlmBufferReader::NtlmBufferReader() = default;

NtlmBufferReader::NtlmBufferReader(base::span<const uint8_t> buffer)
    : buffer_(buffer) {}

NtlmBufferReader::~NtlmBufferReader() = default;

bool NtlmBufferReader::CanRead(size_t len) const {
  return CanReadFrom(GetCursor(), len);
}

bool NtlmBufferReader::CanReadFrom(size_t offset, size_t len) const {
  if (len == 0)
    return true;

  return (len <= GetLength() && offset <= GetLength() - len);
}

bool NtlmBufferReader::ReadUInt16(uint16_t* value) {
  return ReadUInt<uint16_t>(value);
}

bool NtlmBufferReader::ReadUInt32(uint32_t* value) {
  return ReadUInt<uint32_t>(value);
}

bool NtlmBufferReader::ReadUInt64(uint64_t* value) {
  return ReadUInt<uint64_t>(value);
}

bool NtlmBufferReader::ReadFlags(NegotiateFlags* flags) {
  uint32_t raw;
  if (!ReadUInt32(&raw))
    return false;

  *flags = static_cast<NegotiateFlags>(raw);
  return true;
}

bool NtlmBufferReader::ReadBytes(base::span<uint8_t> buffer) {
  if (!CanRead(buffer.size()))
    return false;

  if (buffer.empty())
    return true;

  memcpy(buffer.data(), GetBufferAtCursor(), buffer.size());

  AdvanceCursor(buffer.size());
  return true;
}

bool NtlmBufferReader::ReadBytesFrom(const SecurityBuffer& sec_buf,
                                     base::span<uint8_t> buffer) {
  if (!CanReadFrom(sec_buf) || buffer.size() < sec_buf.length)
    return false;

  if (buffer.empty())
    return true;

  memcpy(buffer.data(), GetBufferPtr() + sec_buf.offset, sec_buf.length);

  return true;
}

bool NtlmBufferReader::ReadPayloadAsBufferReader(const SecurityBuffer& sec_buf,
                                                 NtlmBufferReader* reader) {
  if (!CanReadFrom(sec_buf))
    return false;

  *reader = NtlmBufferReader(
      base::make_span(GetBufferPtr() + sec_buf.offset, sec_buf.length));
  return true;
}

bool NtlmBufferReader::ReadSecurityBuffer(SecurityBuffer* sec_buf) {
  return ReadUInt16(&sec_buf->length) && SkipBytes(sizeof(uint16_t)) &&
         ReadUInt32(&sec_buf->offset);
}

bool NtlmBufferReader::ReadAvPairHeader(TargetInfoAvId* avid, uint16_t* avlen) {
  if (!CanRead(kAvPairHeaderLen))
    return false;

  uint16_t raw_avid;
  bool result = ReadUInt16(&raw_avid) && ReadUInt16(avlen);
  DCHECK(result);

  // Don't try and validate the avid because the code only cares about a few
  // specific ones and it is likely a future version might extend this field.
  // The implementation can ignore and skip over AV Pairs it doesn't
  // understand.
  *avid = static_cast<TargetInfoAvId>(raw_avid);

  return true;
}

bool NtlmBufferReader::ReadTargetInfo(size_t target_info_len,
                                      std::vector<AvPair>* av_pairs) {
  DCHECK(av_pairs->empty());

  // A completely empty target info is allowed.
  if (target_info_len == 0)
    return true;

  // If there is any content there has to be at least one terminating header.
  if (!CanRead(target_info_len) || target_info_len < kAvPairHeaderLen) {
    return false;
  }

  size_t target_info_end = GetCursor() + target_info_len;
  bool saw_eol = false;

  while ((GetCursor() < target_info_end)) {
    AvPair pair;
    if (!ReadAvPairHeader(&pair.avid, &pair.avlen))
      break;

    // Make sure the length wouldn't read outside the buffer.
    if (!CanRead(pair.avlen))
      return false;

    // Take a copy of the payload in the AVPair.
    pair.buffer.assign(GetBufferAtCursor(), GetBufferAtCursor() + pair.avlen);
    if (pair.avid == TargetInfoAvId::kEol) {
      // Terminator must have zero length.
      if (pair.avlen != 0)
        return false;

      // Break out of the loop once a valid terminator is found. After the
      // loop it will be validated that the whole target info was consumed.
      saw_eol = true;
      break;
    }

    switch (pair.avid) {
      case TargetInfoAvId::kFlags:
        // For flags also populate the flags field so it doesn't
        // have to be modified through the raw buffer later.
        if (pair.avlen != sizeof(uint32_t) ||
            !ReadUInt32(reinterpret_cast<uint32_t*>(&pair.flags)))
          return false;
        break;
      case TargetInfoAvId::kTimestamp:
        // Populate timestamp so it doesn't need to be read through the
        // raw buffer later.
        if (pair.avlen != sizeof(uint64_t) || !ReadUInt64(&pair.timestamp))
          return false;
        break;
      case TargetInfoAvId::kChannelBindings:
      case TargetInfoAvId::kTargetName:
        // The server should never send these, and with EPA enabled the client
        // will add these to the authenticate message. To avoid issues with
        // duplicates or only one being read, just don't allow them.
        return false;
      default:
        // For all other types, just jump over the payload to the next pair.
        // If there aren't enough bytes left, then fail.
        if (!SkipBytes(pair.avlen))
          return false;
        break;
    }

    av_pairs->push_back(std::move(pair));
  }

  // Fail if the buffer wasn't properly formed. The entire payload should have
  // been consumed and a terminator found.
  if ((GetCursor() != target_info_end) || !saw_eol)
    return false;

  return true;
}

bool NtlmBufferReader::ReadTargetInfoPayload(std::vector<AvPair>* av_pairs) {
  DCHECK(av_pairs->empty());

  SecurityBuffer sec_buf;

  // First read the security buffer.
  if (!ReadSecurityBuffer(&sec_buf))
    return false;

  NtlmBufferReader payload_reader;
  if (!ReadPayloadAsBufferReader(sec_buf, &payload_reader))
    return false;

  if (!payload_reader.ReadTargetInfo(sec_buf.length, av_pairs))
    return false;

  // |ReadTargetInfo| should have consumed the entire contents.
  return payload_reader.IsEndOfBuffer();
}

bool NtlmBufferReader::ReadMessageType(MessageType* message_type) {
  uint32_t raw_message_type;
  if (!ReadUInt32(&raw_message_type))
    return false;

  *message_type = static_cast<MessageType>(raw_message_type);

  if (*message_type != MessageType::kNegotiate &&
      *message_type != MessageType::kChallenge &&
      *message_type != MessageType::kAuthenticate)
    return false;

  return true;
}

bool NtlmBufferReader::SkipSecurityBuffer() {
  return SkipBytes(kSecurityBufferLen);
}

bool NtlmBufferReader::SkipSecurityBufferWithValidation() {
  SecurityBuffer sec_buf;
  return ReadSecurityBuffer(&sec_buf) && CanReadFrom(sec_buf);
}

bool NtlmBufferReader::SkipBytes(size_t count) {
  if (!CanRead(count))
    return false;

  AdvanceCursor(count);
  return true;
}

bool NtlmBufferReader::MatchSignature() {
  if (!CanRead(kSignatureLen))
    return false;

  if (memcmp(kSignature, GetBufferAtCursor(), kSignatureLen) != 0)
    return false;

  AdvanceCursor(kSignatureLen);
  return true;
}

bool NtlmBufferReader::MatchMessageType(MessageType message_type) {
  MessageType actual_message_type;
  return ReadMessageType(&actual_message_type) &&
         (actual_message_type == message_type);
}

bool NtlmBufferReader::MatchMessageHeader(MessageType message_type) {
  return MatchSignature() && MatchMessageType(message_type);
}

bool NtlmBufferReader::MatchZeros(size_t count) {
  if (!CanRead(count))
    return false;

  for (size_t i = 0; i < count; i++) {
    if (GetBufferAtCursor()[i] != 0)
      return false;
  }

  AdvanceCursor(count);
  return true;
}

bool NtlmBufferReader::MatchEmptySecurityBuffer() {
  SecurityBuffer sec_buf;
  return ReadSecurityBuffer(&sec_buf) && (sec_buf.offset <= GetLength()) &&
         (sec_buf.length == 0);
}

template <typename T>
bool NtlmBufferReader::ReadUInt(T* value) {
  size_t int_size = sizeof(T);
  if (!CanRead(int_size))
    return false;

  *value = 0;
  for (size_t i = 0; i < int_size; i++) {
    *value += static_cast<T>(GetByteAtCursor()) << (i * 8);
    AdvanceCursor(1);
  }

  return true;
}

void NtlmBufferReader::SetCursor(size_t cursor) {
  DCHECK_LE(cursor, GetLength());

  cursor_ = cursor;
}

}  // namespace net::ntlm
