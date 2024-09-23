// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm_buffer_reader.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::ntlm {

TEST(NtlmBufferReaderTest, Initialization) {
  const uint8_t buf[1] = {0};
  NtlmBufferReader reader(buf);

  ASSERT_EQ(std::size(buf), reader.GetLength());
  ASSERT_EQ(0u, reader.GetCursor());
  ASSERT_FALSE(reader.IsEndOfBuffer());
  ASSERT_TRUE(reader.CanRead(1));
  ASSERT_FALSE(reader.CanRead(2));
  ASSERT_TRUE(reader.CanReadFrom(0, 1));
  ASSERT_TRUE(reader.CanReadFrom(SecurityBuffer(0, 1)));
  ASSERT_FALSE(reader.CanReadFrom(1, 1));
  ASSERT_FALSE(reader.CanReadFrom(SecurityBuffer(1, 1)));
  ASSERT_FALSE(reader.CanReadFrom(0, 2));
  ASSERT_FALSE(reader.CanReadFrom(SecurityBuffer(0, 2)));

  // With length=0 the offset can be out of bounds.
  ASSERT_TRUE(reader.CanReadFrom(99, 0));
  ASSERT_TRUE(reader.CanReadFrom(SecurityBuffer(99, 0)));
}

TEST(NtlmBufferReaderTest, EmptyBuffer) {
  std::vector<uint8_t> b;
  NtlmBufferReader reader(b);

  ASSERT_EQ(0u, reader.GetCursor());
  ASSERT_EQ(0u, reader.GetLength());
  ASSERT_TRUE(reader.CanRead(0));
  ASSERT_FALSE(reader.CanRead(1));
  ASSERT_TRUE(reader.IsEndOfBuffer());

  // A read from an empty (zero-byte) source into an empty (zero-byte)
  // destination buffer should succeed as a no-op.
  std::vector<uint8_t> dest;
  ASSERT_TRUE(reader.ReadBytes(dest));

  // A read from a non-empty source into an empty (zero-byte) destination
  // buffer should succeed as a no-op.
  std::vector<uint8_t> b2{0x01};
  NtlmBufferReader reader2(b2);
  ASSERT_EQ(0u, reader2.GetCursor());
  ASSERT_EQ(1u, reader2.GetLength());

  ASSERT_TRUE(reader2.CanRead(0));
  ASSERT_TRUE(reader2.ReadBytes(dest));

  ASSERT_EQ(0u, reader2.GetCursor());
  ASSERT_EQ(1u, reader2.GetLength());
}

TEST(NtlmBufferReaderTest, NullBuffer) {
  NtlmBufferReader reader;

  ASSERT_EQ(0u, reader.GetCursor());
  ASSERT_EQ(0u, reader.GetLength());
  ASSERT_TRUE(reader.CanRead(0));
  ASSERT_FALSE(reader.CanRead(1));
  ASSERT_TRUE(reader.IsEndOfBuffer());

  // A read from a null source into an empty (zero-byte) destination buffer
  // should succeed as a no-op.
  std::vector<uint8_t> dest;
  ASSERT_TRUE(reader.ReadBytes(dest));
}

TEST(NtlmBufferReaderTest, Read16) {
  const uint8_t buf[2] = {0x22, 0x11};
  const uint16_t expected = 0x1122;

  NtlmBufferReader reader(buf);

  uint16_t actual;
  ASSERT_TRUE(reader.ReadUInt16(&actual));
  ASSERT_EQ(expected, actual);
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadUInt16(&actual));
}

TEST(NtlmBufferReaderTest, Read32) {
  const uint8_t buf[4] = {0x44, 0x33, 0x22, 0x11};
  const uint32_t expected = 0x11223344;

  NtlmBufferReader reader(buf);

  uint32_t actual;
  ASSERT_TRUE(reader.ReadUInt32(&actual));
  ASSERT_EQ(expected, actual);
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadUInt32(&actual));
}

TEST(NtlmBufferReaderTest, Read64) {
  const uint8_t buf[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  const uint64_t expected = 0x1122334455667788;

  NtlmBufferReader reader(buf);

  uint64_t actual;
  ASSERT_TRUE(reader.ReadUInt64(&actual));
  ASSERT_EQ(expected, actual);
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadUInt64(&actual));
}

TEST(NtlmBufferReaderTest, ReadBytes) {
  const uint8_t expected[8] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  uint8_t actual[8];

  NtlmBufferReader reader(expected);

  ASSERT_TRUE(reader.ReadBytes(actual));
  ASSERT_EQ(0, memcmp(actual, expected, std::size(actual)));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadBytes(base::make_span(actual, 1u)));
}

TEST(NtlmBufferReaderTest, ReadSecurityBuffer) {
  const uint8_t buf[8] = {0x22, 0x11, 0xFF, 0xEE, 0x88, 0x77, 0x66, 0x55};
  const uint16_t length = 0x1122;
  const uint32_t offset = 0x55667788;

  NtlmBufferReader reader(buf);

  SecurityBuffer sec_buf;
  ASSERT_TRUE(reader.ReadSecurityBuffer(&sec_buf));
  ASSERT_EQ(length, sec_buf.length);
  ASSERT_EQ(offset, sec_buf.offset);
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadSecurityBuffer(&sec_buf));
}

TEST(NtlmBufferReaderTest, ReadSecurityBufferPastEob) {
  const uint8_t buf[7] = {0};
  NtlmBufferReader reader(buf);

  SecurityBuffer sec_buf;
  ASSERT_FALSE(reader.ReadSecurityBuffer(&sec_buf));
}

TEST(NtlmBufferReaderTest, ReadPayloadAsBufferReader) {
  const uint8_t buf[8] = {0xff, 0xff, 0x11, 0x22, 0x33, 0x44, 0xff, 0xff};
  const uint32_t expected = 0x44332211;
  NtlmBufferReader reader(buf);
  ASSERT_EQ(0u, reader.GetCursor());

  // Create a security buffer with offset 2 and length 4.
  SecurityBuffer sec_buf(2, 4);
  NtlmBufferReader sub_reader;
  ASSERT_EQ(0u, sub_reader.GetLength());
  ASSERT_EQ(0u, sub_reader.GetCursor());

  // Read the 4 non-0xff bytes from the middle of |buf|.
  ASSERT_TRUE(reader.ReadPayloadAsBufferReader(sec_buf, &sub_reader));

  // |reader| cursor should not move.
  ASSERT_EQ(0u, reader.GetCursor());
  ASSERT_EQ(sec_buf.length, sub_reader.GetLength());
  ASSERT_EQ(0u, sub_reader.GetCursor());

  // Read from the payload in |sub_reader|.
  uint32_t actual;
  ASSERT_TRUE(sub_reader.ReadUInt32(&actual));
  ASSERT_EQ(expected, actual);
  ASSERT_TRUE(sub_reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, ReadPayloadBadOffset) {
  const uint8_t buf[4] = {0};
  NtlmBufferReader reader(buf);

  NtlmBufferReader sub_reader;
  ASSERT_FALSE(
      reader.ReadPayloadAsBufferReader(SecurityBuffer(4, 1), &sub_reader));
}

TEST(NtlmBufferReaderTest, ReadPayloadBadLength) {
  const uint8_t buf[4] = {0};
  NtlmBufferReader reader(buf);

  NtlmBufferReader sub_reader;
  ASSERT_FALSE(
      reader.ReadPayloadAsBufferReader(SecurityBuffer(3, 2), &sub_reader));
}

TEST(NtlmBufferReaderTest, SkipSecurityBuffer) {
  const uint8_t buf[kSecurityBufferLen] = {0};

  NtlmBufferReader reader(buf);
  ASSERT_TRUE(reader.SkipSecurityBuffer());
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.SkipSecurityBuffer());
}

TEST(NtlmBufferReaderTest, SkipSecurityBufferPastEob) {
  // The buffer is one byte shorter than security buffer.
  const uint8_t buf[kSecurityBufferLen - 1] = {0};

  NtlmBufferReader reader(buf);
  ASSERT_FALSE(reader.SkipSecurityBuffer());
}

TEST(NtlmBufferReaderTest, SkipSecurityBufferWithValidationEmpty) {
  const uint8_t buf[kSecurityBufferLen] = {0, 0, 0, 0, 0, 0, 0, 0};

  NtlmBufferReader reader(buf);
  ASSERT_TRUE(reader.SkipSecurityBufferWithValidation());
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.SkipSecurityBufferWithValidation());
}

TEST(NtlmBufferReaderTest, SkipSecurityBufferWithValidationValid) {
  // A valid security buffer that points to the 1 payload byte.
  const uint8_t buf[kSecurityBufferLen + 1] = {
      0x01, 0, 0x01, 0, kSecurityBufferLen, 0, 0, 0, 0xFF};

  NtlmBufferReader reader(buf);
  ASSERT_TRUE(reader.SkipSecurityBufferWithValidation());
  ASSERT_EQ(kSecurityBufferLen, reader.GetCursor());
  ASSERT_FALSE(reader.SkipSecurityBufferWithValidation());
}

TEST(NtlmBufferReaderTest,
     SkipSecurityBufferWithValidationPayloadLengthPastEob) {
  // Security buffer with length that points past the end of buffer.
  const uint8_t buf[kSecurityBufferLen + 1] = {
      0x02, 0, 0x02, 0, kSecurityBufferLen, 0, 0, 0, 0xFF};

  NtlmBufferReader reader(buf);
  ASSERT_FALSE(reader.SkipSecurityBufferWithValidation());
}

TEST(NtlmBufferReaderTest,
     SkipSecurityBufferWithValidationPayloadOffsetPastEob) {
  // Security buffer with offset that points past the end of buffer.
  const uint8_t buf[kSecurityBufferLen + 1] = {
      0x02, 0, 0x02, 0, kSecurityBufferLen + 1, 0, 0, 0, 0xFF};

  NtlmBufferReader reader(buf);
  ASSERT_FALSE(reader.SkipSecurityBufferWithValidation());
}

TEST(NtlmBufferReaderTest,
     SkipSecurityBufferWithValidationZeroLengthPayloadOffsetPastEob) {
  // Security buffer with offset that points past the end of buffer but
  // length is 0.
  const uint8_t buf[kSecurityBufferLen] = {0, 0, 0, 0, kSecurityBufferLen + 1,
                                           0, 0, 0};

  NtlmBufferReader reader(buf);
  ASSERT_TRUE(reader.SkipSecurityBufferWithValidation());
  ASSERT_EQ(kSecurityBufferLen, reader.GetCursor());
}

TEST(NtlmBufferReaderTest, SkipBytes) {
  const uint8_t buf[8] = {0};

  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.SkipBytes(std::size(buf)));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.SkipBytes(std::size(buf)));
}

TEST(NtlmBufferReaderTest, SkipBytesPastEob) {
  const uint8_t buf[8] = {0};

  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.SkipBytes(std::size(buf) + 1));
}

TEST(NtlmBufferReaderTest, MatchSignatureTooShort) {
  const uint8_t buf[7] = {0};

  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.CanRead(7));
  ASSERT_FALSE(reader.MatchSignature());
}

TEST(NtlmBufferReaderTest, MatchSignatureNoMatch) {
  // The last byte should be a 0.
  const uint8_t buf[8] = {'N', 'T', 'L', 'M', 'S', 'S', 'P', 0xff};
  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.CanRead(8));
  ASSERT_FALSE(reader.MatchSignature());
}

TEST(NtlmBufferReaderTest, MatchSignatureOk) {
  const uint8_t buf[8] = {'N', 'T', 'L', 'M', 'S', 'S', 'P', 0};
  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchSignature());
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, ReadInvalidMessageType) {
  // Only 0x01, 0x02, and 0x03 are valid message types.
  const uint8_t buf[4] = {0x04, 0, 0, 0};
  NtlmBufferReader reader(buf);

  MessageType message_type;
  ASSERT_FALSE(reader.ReadMessageType(&message_type));
}

TEST(NtlmBufferReaderTest, ReadMessageTypeNegotiate) {
  const uint8_t buf[4] = {static_cast<uint8_t>(MessageType::kNegotiate), 0, 0,
                          0};
  NtlmBufferReader reader(buf);

  MessageType message_type;
  ASSERT_TRUE(reader.ReadMessageType(&message_type));
  ASSERT_EQ(MessageType::kNegotiate, message_type);
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, ReadMessageTypeChallenge) {
  const uint8_t buf[4] = {static_cast<uint8_t>(MessageType::kChallenge), 0, 0,
                          0};
  NtlmBufferReader reader(buf);

  MessageType message_type;
  ASSERT_TRUE(reader.ReadMessageType(&message_type));
  ASSERT_EQ(MessageType::kChallenge, message_type);
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, ReadTargetInfoEolOnly) {
  // Buffer contains only an EOL terminator.
  const uint8_t buf[4] = {0, 0, 0, 0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_TRUE(av_pairs.empty());
}

TEST(NtlmBufferReaderTest, ReadTargetInfoEmpty) {
  NtlmBufferReader reader;

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfo(0, &av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_TRUE(av_pairs.empty());
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTimestampAndEolOnly) {
  // Buffer contains a timestamp av pair and an EOL terminator.
  const uint8_t buf[16] = {0x07, 0,    0x08, 0,    0x11, 0x22, 0x33, 0x44,
                           0x55, 0x66, 0x77, 0x88, 0,    0,    0,    0};
  const uint64_t expected_timestamp = 0x8877665544332211;

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_EQ(1u, av_pairs.size());

  // Verify the timestamp av pair.
  ASSERT_EQ(TargetInfoAvId::kTimestamp, av_pairs[0].avid);
  ASSERT_EQ(sizeof(uint64_t), av_pairs[0].avlen);
  ASSERT_EQ(sizeof(uint64_t), av_pairs[0].buffer.size());
  ASSERT_EQ(expected_timestamp, av_pairs[0].timestamp);
}

TEST(NtlmBufferReaderTest, ReadTargetInfoFlagsAndEolOnly) {
  // Buffer contains a flags av pair with the MIC bit and an EOL terminator.
  const uint8_t buf[12] = {0x06, 0, 0x04, 0, 0x02, 0, 0, 0, 0, 0, 0, 0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_EQ(1u, av_pairs.size());

  // Verify the flags av pair.
  ASSERT_EQ(TargetInfoAvId::kFlags, av_pairs[0].avid);
  ASSERT_EQ(sizeof(TargetInfoAvFlags), av_pairs[0].avlen);
  ASSERT_EQ(TargetInfoAvFlags::kMicPresent, av_pairs[0].flags);
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTooSmall) {
  // Target info must least contain enough space for a terminator pair.
  const uint8_t buf[3] = {0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoInvalidTimestampSize) {
  // Timestamps must be 64 bits/8 bytes. A timestamp av pair with a
  // different length is invalid.
  const uint8_t buf[15] = {0x07, 0,    0x07, 0, 0x11, 0x22, 0x33, 0x44,
                           0x55, 0x66, 0x77, 0, 0,    0,    0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoInvalidTimestampPastEob) {
  // The timestamp avlen is correct but would read past the end of the buffer.
  const uint8_t buf[11] = {0x07, 0,    0x08, 0,    0x11, 0x22,
                           0x33, 0x44, 0x55, 0x66, 0x77};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoOtherField) {
  // A domain name AvPair containing the string L'ABCD' followed by
  // a terminating AvPair.
  const uint8_t buf[16] = {0x02, 0, 0x08, 0, 'A', 0, 'B', 0,
                           'C',  0, 'D',  0, 0,   0, 0,   0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_EQ(1u, av_pairs.size());

  // Verify the domain name AvPair.
  ASSERT_EQ(TargetInfoAvId::kDomainName, av_pairs[0].avid);
  ASSERT_EQ(8, av_pairs[0].avlen);
  ASSERT_EQ(0, memcmp(buf + 4, av_pairs[0].buffer.data(), 8));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoNoTerminator) {
  // A domain name AvPair containing the string L'ABCD' but there is no
  // terminating AvPair.
  const uint8_t buf[12] = {0x02, 0, 0x08, 0, 'A', 0, 'B', 0, 'C', 0, 'D', 0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTerminatorAtLocationOtherThanEnd) {
  // Target info contains [flags, terminator, domain, terminator]. This
  // should fail because the terminator should only appear at the end.
  const uint8_t buf[] = {0x06, 0, 0x04, 0, 0x02, 0, 0,   0, 0,   0,
                         0,    0, 0x02, 0, 0x08, 0, 'A', 0, 'B', 0,
                         'C',  0, 'D',  0, 0,    0, 0,   0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTerminatorNonZeroLength) {
  // A flags Av Pair followed by a terminator pair with a non-zero length.
  const uint8_t buf[] = {0x06, 0, 0x04, 0, 0x02, 0, 0, 0, 0, 0, 0x01, 0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTerminatorNonZeroLength2) {
  // A flags Av Pair followed by a terminator pair with a non-zero length,
  // but otherwise in bounds payload. Terminator pairs must have zero
  // length, so this is not valid.
  const uint8_t buf[] = {0x06, 0,    0x04, 0,    0x02, 0, 0, 0, 0,
                         0,    0x01, 0,    0xff, 0,    0, 0, 0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfo(std::size(buf), &av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoEmptyPayload) {
  // Security buffer with no payload.
  const uint8_t buf[] = {0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfoPayload(&av_pairs));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_TRUE(av_pairs.empty());
}

TEST(NtlmBufferReaderTest, ReadTargetInfoEolOnlyPayload) {
  // Security buffer with an EOL payload
  const uint8_t buf[] = {0x04, 0x00, 0x04, 0x00, 0x08, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfoPayload(&av_pairs));
  ASSERT_FALSE(reader.IsEndOfBuffer());

  // Should only have advanced over the security buffer.
  ASSERT_EQ(kSecurityBufferLen, reader.GetCursor());
  ASSERT_TRUE(av_pairs.empty());
}

TEST(NtlmBufferReaderTest, ReadTargetInfoTooShortPayload) {
  // Security buffer with a payload too small to contain any pairs.
  const uint8_t buf[] = {0x03, 0x00, 0x03, 0x00, 0x08, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_FALSE(reader.ReadTargetInfoPayload(&av_pairs));
}

TEST(NtlmBufferReaderTest, ReadTargetInfoFlagsPayload) {
  // Security buffer followed by a 12 byte payload containing a flags AvPair
  // with the MIC bit, followed by a terminator pair.
  const uint8_t buf[] = {0x0c, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x00,
                         0x00, 0x06, 0,    0x04, 0,    0x02, 0,
                         0,    0,    0,    0,    0,    0};

  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfoPayload(&av_pairs));
  ASSERT_FALSE(reader.IsEndOfBuffer());

  // Should only have advanced over the security buffer.
  ASSERT_EQ(kSecurityBufferLen, reader.GetCursor());

  // Contains a single flags AVPair containing the MIC bit.
  ASSERT_EQ(1u, av_pairs.size());
  ASSERT_EQ(TargetInfoAvFlags::kMicPresent, av_pairs[0].flags);
}

TEST(NtlmBufferReaderTest, ReadTargetInfoFlagsPayloadWithPaddingBetween) {
  // Security buffer followed by a 12 byte payload containing a flags AvPair
  // with the MIC bit, followed by a terminator pair. 5 bytes of 0xff padding
  // are between the SecurityBuffer and the payload to test when the payload
  // is not contiguous.
  const uint8_t buf[] = {0x0c, 0x00, 0x0c, 0x00, 0x0c, 0x00, 0x00, 0x00,
                         0xff, 0xff, 0xff, 0xff, 0x06, 0,    0x04, 0,
                         0x02, 0,    0,    0,    0,    0,    0,    0};
  NtlmBufferReader reader(buf);

  std::vector<AvPair> av_pairs;
  ASSERT_TRUE(reader.ReadTargetInfoPayload(&av_pairs));
  ASSERT_FALSE(reader.IsEndOfBuffer());

  // Should only have advanced over the security buffer.
  ASSERT_EQ(kSecurityBufferLen, reader.GetCursor());

  // Contains a single flags AVPair containing the MIC bit.
  ASSERT_EQ(1u, av_pairs.size());
  ASSERT_EQ(TargetInfoAvFlags::kMicPresent, av_pairs[0].flags);
}

TEST(NtlmBufferReaderTest, ReadMessageTypeAuthenticate) {
  const uint8_t buf[4] = {static_cast<uint8_t>(MessageType::kAuthenticate), 0,
                          0, 0};
  NtlmBufferReader reader(buf);

  MessageType message_type;
  ASSERT_TRUE(reader.ReadMessageType(&message_type));
  ASSERT_EQ(MessageType::kAuthenticate, message_type);
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, MatchMessageTypeAuthenticate) {
  const uint8_t buf[4] = {static_cast<uint8_t>(MessageType::kAuthenticate), 0,
                          0, 0};
  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchMessageType(MessageType::kAuthenticate));
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, MatchMessageTypeInvalid) {
  // Only 0x01, 0x02, and 0x03 are valid message types.
  const uint8_t buf[4] = {0x04, 0, 0, 0};
  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchMessageType(MessageType::kAuthenticate));
}

TEST(NtlmBufferReaderTest, MatchMessageTypeMismatch) {
  const uint8_t buf[4] = {static_cast<uint8_t>(MessageType::kChallenge), 0, 0,
                          0};
  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchMessageType(MessageType::kAuthenticate));
}

TEST(NtlmBufferReaderTest, MatchAuthenticateHeader) {
  const uint8_t buf[12] = {
      'N', 'T', 'L',
      'M', 'S', 'S',
      'P', 0,   static_cast<uint8_t>(MessageType::kAuthenticate),
      0,   0,   0};
  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchMessageHeader(MessageType::kAuthenticate));
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, MatchAuthenticateHeaderMisMatch) {
  const uint8_t buf[12] = {
      'N', 'T', 'L',
      'M', 'S', 'S',
      'P', 0,   static_cast<uint8_t>(MessageType::kChallenge),
      0,   0,   0};
  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchMessageType(MessageType::kAuthenticate));
}

TEST(NtlmBufferReaderTest, MatchZeros) {
  const uint8_t buf[6] = {0, 0, 0, 0, 0, 0};

  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchZeros(std::size(buf)));
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.MatchZeros(1));
}

TEST(NtlmBufferReaderTest, MatchZerosFail) {
  const uint8_t buf[6] = {0, 0, 0, 0, 0, 0xFF};

  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchZeros(std::size(buf)));
}

TEST(NtlmBufferReaderTest, MatchEmptySecurityBuffer) {
  const uint8_t buf[kSecurityBufferLen] = {0, 0, 0, 0, 0, 0, 0, 0};

  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchEmptySecurityBuffer());
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.MatchEmptySecurityBuffer());
}

TEST(NtlmBufferReaderTest, MatchEmptySecurityBufferLengthZeroOffsetEnd) {
  const uint8_t buf[kSecurityBufferLen] = {0, 0, 0, 0, 0x08, 0, 0, 0};

  NtlmBufferReader reader(buf);

  ASSERT_TRUE(reader.MatchEmptySecurityBuffer());
  ASSERT_TRUE(reader.IsEndOfBuffer());
}

TEST(NtlmBufferReaderTest, MatchEmptySecurityBufferLengthZeroPastEob) {
  const uint8_t buf[kSecurityBufferLen] = {0, 0, 0, 0, 0x09, 0, 0, 0};

  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchEmptySecurityBuffer());
}

TEST(NtlmBufferReaderTest, MatchEmptySecurityBufferLengthNonZeroLength) {
  const uint8_t buf[kSecurityBufferLen + 1] = {0x01, 0, 0, 0,   0x08,
                                               0,    0, 0, 0xff};

  NtlmBufferReader reader(buf);

  ASSERT_FALSE(reader.MatchEmptySecurityBuffer());
}

TEST(NtlmBufferReaderTest, ReadAvPairHeader) {
  const uint8_t buf[4] = {0x06, 0x00, 0x11, 0x22};

  NtlmBufferReader reader(buf);

  TargetInfoAvId actual_avid;
  uint16_t actual_avlen;
  ASSERT_TRUE(reader.ReadAvPairHeader(&actual_avid, &actual_avlen));
  ASSERT_EQ(TargetInfoAvId::kFlags, actual_avid);
  ASSERT_EQ(0x2211, actual_avlen);
  ASSERT_TRUE(reader.IsEndOfBuffer());
  ASSERT_FALSE(reader.ReadAvPairHeader(&actual_avid, &actual_avlen));
}

TEST(NtlmBufferReaderTest, ReadAvPairHeaderPastEob) {
  const uint8_t buf[3] = {0x06, 0x00, 0x11};

  NtlmBufferReader reader(buf);

  TargetInfoAvId avid;
  uint16_t avlen;
  ASSERT_FALSE(reader.ReadAvPairHeader(&avid, &avlen));
}

}  // namespace net::ntlm
