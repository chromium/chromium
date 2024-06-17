// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef NET_NTLM_NTLM_BUFFER_READER_H_
#define NET_NTLM_NTLM_BUFFER_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "net/base/net_export.h"
#include "net/ntlm/ntlm_constants.h"

namespace net::ntlm {

// Supports various bounds-checked low level buffer operations required by an
// NTLM implementation.
//
// The class supports the sequential read of a provided buffer. All reads
// perform bounds checking to ensure enough space is remaining in the buffer.
//
// Read* methods read from the buffer at the current cursor position and
// perform any necessary type conversion and provide the data in out params.
// After a successful read the cursor position is advanced past the read
// field.
//
// Failed Read*s or Match*s leave the cursor in an undefined position and the
// buffer MUST be discarded with no further operations performed.
//
// Read*Payload methods first reads a security buffer (see
// |ReadSecurityBuffer|), then reads the requested payload from the offset
// and length stated in the security buffer.
//
// If the length and offset in the security buffer would cause a read outside
// the message buffer the payload will not be read and the function will
// return false.
//
// Based on [MS-NLMP]: NT LAN Manager (NTLM) Authentication Protocol
// Specification version 28.0 [1]. Additional NTLM reference [2].
//
// [1] https://msdn.microsoft.com/en-us/library/cc236621.aspx
// [2] http://davenport.sourceforge.net/ntlm.html
class NET_EXPORT_PRIVATE NtlmBufferReader {
 public:
  NtlmBufferReader();
  // |buffer| is not copied and must outlive the |NtlmBufferReader|.
  explicit NtlmBufferReader(base::span<const uint8_t> buffer);

  ~NtlmBufferReader();

  size_t GetLength() const { return buffer_.size(); }
  size_t GetCursor() const { return cursor_; }
  bool IsEndOfBuffer() const { return cursor_ >= GetLength(); }

  // Returns true if there are |len| more bytes between the current cursor
  // position and the end of the buffer.
  bool CanRead(size_t len) const;

  // Returns true if there are |len| more bytes between |offset| and the end
  // of the buffer. The cursor position is not used or modified.
  bool CanReadFrom(size_t offset, size_t len) const;

  // Returns true if it would be possible to read the payload described by the
  // security buffer.
  bool CanReadFrom(SecurityBuffer sec_buf) const {
    return CanReadFrom(sec_buf.offset, sec_buf.length);
  }

  // Reads a 16 bit value (little endian) as a uint16_t. If there are not 16
  // more bits available, it returns false.
  [[nodiscard]] bool ReadUInt16(uint16_t* value);

  // Reads a 32 bit value (little endian) as a uint32_t. If there are not 32
  // more bits available, it returns false.
  [[nodiscard]] bool ReadUInt32(uint32_t* value);

  // Reads a 64 bit value (little endian) as a uint64_t. If there are not 64
  // more bits available, it returns false.
  [[nodiscard]] bool ReadUInt64(uint64_t* value);

  // Calls |ReadUInt32| and returns it cast as |NegotiateFlags|. No
  // validation of the value takes place.
  [[nodiscard]] bool ReadFlags(NegotiateFlags* flags);

  // Reads |len| bytes and copies them into |buffer|.
  [[nodiscard]] bool ReadBytes(base::span<uint8_t> buffer);

  // Reads |sec_buf.length| bytes from offset |sec_buf.offset| and copies them
  // into |buffer|. If the security buffer specifies a payload outside the
  // buffer, then the call fails. Unlike the other Read* methods, this does
  // not move the cursor.
  [[nodiscard]] bool ReadBytesFrom(const SecurityBuffer& sec_buf,
                                   base::span<uint8_t> buffer);

  // Reads |sec_buf.length| bytes from offset |sec_buf.offset| and assigns
  // |reader| an |NtlmBufferReader| representing the payload. If the security
  //  buffer specifies a payload outside the buffer, then the call fails, and
  // the state of |reader| is undefined. Unlike the other Read* methods, this
  // does not move the cursor.
  [[nodiscard]] bool ReadPayloadAsBufferReader(const SecurityBuffer& sec_buf,
                                               NtlmBufferReader* reader);

  // A security buffer is an 8 byte structure that defines the offset and
  // length of a payload (string, struct or byte array) that appears after the
  // fixed part of the message.
  //
  // The structure is (little endian fields):
  //     uint16 - |length| Length of payload
  //     uint16 - Allocation (this is always ignored and not returned)
  //     uint32 - |offset| Offset from start of message
  [[nodiscard]] bool ReadSecurityBuffer(SecurityBuffer* sec_buf);

  // Reads an AvPair header. AvPairs appear sequentially, terminated by a
  // special EOL AvPair, in the target info payload of the Challenge message.
  // See [MS-NLMP] Section 2.2.2.1.
  //
  // An AvPair contains an inline payload, and has the structure below (
  // little endian fields):
  //    uint16      - AvID: Identifies the type of the payload.
  //    uint16      - AvLen: The length of the following payload.
  //    (variable)  - Payload: Variable length payload. The content and
  //                  format are determined by the AvId.
  [[nodiscard]] bool ReadAvPairHeader(TargetInfoAvId* avid, uint16_t* avlen);

  // There are 3 message types Negotiate (sent by client), Challenge (sent by
  // server), and Authenticate (sent by client).
  //
  // This reads the message type from the header and will return false if the
  // value is invalid.
  [[nodiscard]] bool ReadMessageType(MessageType* message_type);

  // Reads |target_info_len| bytes and parses them as a sequence of Av Pairs.
  // |av_pairs| should be empty on entry to this function. If |ReadTargetInfo|
  // returns false, the content of |av_pairs| is in an undefined state and
  // should be discarded.
  [[nodiscard]] bool ReadTargetInfo(size_t target_info_len,
                                    std::vector<AvPair>* av_pairs);

  // Reads a security buffer, then parses the security buffer payload as a
  // target info. The target info is returned as a sequence of AvPairs, with
  // the terminating AvPair omitted. A zero length payload is valid and will
  // result in an empty list in |av_pairs|. Any non-zero length payload must
  // have a terminating AvPair.
  // |av_pairs| should be empty on entry to this function. If |ReadTargetInfo|
  // returns false, the content of |av_pairs| is in an undefined state and
  // should be discarded.
  [[nodiscard]] bool ReadTargetInfoPayload(std::vector<AvPair>* av_pairs);

  // Skips over a security buffer field without reading the fields. This is
  // the equivalent of advancing the cursor 8 bytes. Returns false if there
  // are less than 8 bytes left in the buffer.
  [[nodiscard]] bool SkipSecurityBuffer();

  // Skips over the security buffer without returning the values, but fails if
  // the values would cause a read outside the buffer if the payload was
  // actually read.
  [[nodiscard]] bool SkipSecurityBufferWithValidation();

  // Skips over |count| bytes in the buffer. Returns false if there are not
  // |count| bytes left in the buffer.
  [[nodiscard]] bool SkipBytes(size_t count);

  // Reads and returns true if the next 8 bytes matches the signature in an
  // NTLM message "NTLMSSP\0". The cursor advances if the the signature
  // is matched.
  [[nodiscard]] bool MatchSignature();

  // Performs |ReadMessageType| and returns true if the value is
  // |message_type|. If the read fails or the message type does not match,
  // the buffer is invalid and MUST be discarded.
  [[nodiscard]] bool MatchMessageType(MessageType message_type);

  // Performs |MatchSignature| then |MatchMessageType|.
  [[nodiscard]] bool MatchMessageHeader(MessageType message_type);

  // Performs |ReadBytes(count)| and returns true if the contents is all
  // zero.
  [[nodiscard]] bool MatchZeros(size_t count);

  // Reads the security buffer and returns true if the length is 0 and
  // the offset is within the message. On failure, the buffer is invalid
  // and MUST be discarded.
  [[nodiscard]] bool MatchEmptySecurityBuffer();

 private:
  // Reads |sizeof(T)| bytes of an integer type from a little-endian buffer.
  template <typename T>
  bool ReadUInt(T* value);

  // Sets the cursor position. The caller should use |GetLength|, |CanRead|,
  // or |CanReadFrom| to verify the bounds before calling this method.
  void SetCursor(size_t cursor);

  // Advances the cursor by |count| bytes. The caller should use |GetLength|,
  // |CanRead|, or |CanReadFrom| to verify the bounds before calling this
  // method.
  void AdvanceCursor(size_t count) { SetCursor(GetCursor() + count); }

  // Returns a constant pointer to the start of the buffer.
  const uint8_t* GetBufferPtr() const { return buffer_.data(); }

  // Returns a pointer to the underlying buffer at the current cursor
  // position.
  const uint8_t* GetBufferAtCursor() const { return GetBufferPtr() + cursor_; }

  // Returns the byte at the current cursor position.
  uint8_t GetByteAtCursor() const {
    DCHECK(!IsEndOfBuffer());
    return *(GetBufferAtCursor());
  }

  base::raw_span<const uint8_t> buffer_;
  size_t cursor_ = 0;
};

}  // namespace net::ntlm

#endif  // NET_NTLM_NTLM_BUFFER_READER_H_
