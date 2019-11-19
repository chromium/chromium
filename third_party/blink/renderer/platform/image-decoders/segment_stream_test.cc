// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/segment_stream.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

// SegmentStream has 4 accessors which do not alter state:
// - isCleared()
// - isAtEnd()
// - getPosition()
// - getLength()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.
//
// There are actually 2 more accessors:
// - hasPosition()
// - hasLength()
// but these should always return true to indicate that we can call getLength()
// for example. So let's not add them to every state changing operation and add
// needless complexity.

namespace blink {

namespace {

constexpr size_t kBufferAllocationSize = 20;
constexpr size_t kInsideBufferPosition = 10;
constexpr size_t kPastEndOfBufferPosition = 30;

::testing::AssertionResult IsCleared(const SegmentStream&);
::testing::AssertionResult IsAtEnd(const SegmentStream&);
::testing::AssertionResult PositionIsZero(const SegmentStream&);
::testing::AssertionResult PositionIsInsideBuffer(const SegmentStream&);
::testing::AssertionResult PositionIsAtEndOfBuffer(const SegmentStream&);
::testing::AssertionResult LengthIsZero(const SegmentStream&);
::testing::AssertionResult LengthIsAllocationSize(const SegmentStream&);

// Many of these tests require a SegmentStream with populated data.
//
// This function creates a buffer of size |kBufferAllocationSize| and prepares
// a SegmentStream with that buffer.
// This also populates other properties such as the length, cleared state, etc.
SegmentStream CreatePopulatedSegmentStream();

// This function creates a buffer of size |kBufferAllocationSize| to be used
// when populating a SegmentStream.
scoped_refptr<SegmentReader> CreateSegmentReader();

size_t ReadFromSegmentStream(SegmentStream&,
                             size_t amount_to_read = kInsideBufferPosition);
size_t PeekIntoSegmentStream(SegmentStream&,
                             size_t amount_to_peek = kInsideBufferPosition);

}  // namespace

TEST(SegmentStreamTest, DefaultConstructorShouldSetIsCleared) {
  SegmentStream segment_stream;

  ASSERT_TRUE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, DefaultConstructorShouldSetIsAtEnd) {
  SegmentStream segment_stream;

  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, DefaultContructorShouldClearPosition) {
  SegmentStream segment_stream;

  ASSERT_TRUE(PositionIsZero(segment_stream));
}

TEST(SegmentStreamTest, DefaultConstructorShouldHaveZeroLength) {
  SegmentStream segment_stream;

  ASSERT_TRUE(LengthIsZero(segment_stream));
}

TEST(SegmentStreamTest, MoveConstructorShouldSetIsClearedWhenRhsIsCleared) {
  SegmentStream cleared_segment_stream;
  ASSERT_TRUE(IsCleared(cleared_segment_stream));

  SegmentStream move_constructed_segment_stream =
      std::move(cleared_segment_stream);

  ASSERT_TRUE(IsCleared(move_constructed_segment_stream));
}

TEST(SegmentStreamTest,
     MoveConstructorShouldUnsetIsClearedWhenRhsIsNotCleared) {
  SegmentStream uncleared_segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsCleared(uncleared_segment_stream));

  SegmentStream move_constructed_segment_stream =
      std::move(uncleared_segment_stream);

  ASSERT_FALSE(IsCleared(move_constructed_segment_stream));
}

TEST(SegmentStreamTest, MoveConstructorShouldSetIsAtEndWhenRhsIsAtEnd) {
  SegmentStream at_end_segment_stream;
  ASSERT_TRUE(IsAtEnd(at_end_segment_stream));

  SegmentStream move_constructed_segment_stream =
      std::move(at_end_segment_stream);

  ASSERT_TRUE(IsAtEnd(move_constructed_segment_stream));
}

TEST(SegmentStreamTest, MoveConstructorShouldUnsetIsAtEndWhenRhsIsNotAtEnd) {
  SegmentStream not_at_end_segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(not_at_end_segment_stream));

  SegmentStream move_constructed_segment_stream =
      std::move(not_at_end_segment_stream);

  ASSERT_FALSE(IsAtEnd(move_constructed_segment_stream));
}

TEST(SegmentStreamTest, MoveContructorShouldCopyRhsPosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  segment_stream.seek(kInsideBufferPosition);
  ASSERT_EQ(kInsideBufferPosition, segment_stream.getPosition());

  SegmentStream move_constructed_segment_stream = std::move(segment_stream);

  ASSERT_EQ(kInsideBufferPosition,
            move_constructed_segment_stream.getPosition());
}

TEST(SegmentStreamTest, MoveConstructorShouldCopyRhsLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());

  SegmentStream move_constructed_segment_stream = std::move(segment_stream);

  ASSERT_EQ(kBufferAllocationSize, move_constructed_segment_stream.getLength());
}

TEST(SegmentStreamTest,
     MoveAssignmentOperatorShouldSetIsClearedWhenRhsIsCleared) {
  SegmentStream cleared_segment_stream;
  ASSERT_TRUE(IsCleared(cleared_segment_stream));

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(cleared_segment_stream);

  ASSERT_TRUE(IsCleared(move_assigned_segment_stream));
}

TEST(SegmentStreamTest,
     MoveAssignmentOperatorShouldUnsetIsClearedWhenRhsIsNotCleared) {
  SegmentStream uncleared_segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsCleared(uncleared_segment_stream));

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(uncleared_segment_stream);

  ASSERT_FALSE(IsCleared(move_assigned_segment_stream));
}

TEST(SegmentStreamTest, MoveAssignmentOperatorShouldSetIsAtEndWhenRhsIsAtEnd) {
  SegmentStream at_end_segment_stream;
  ASSERT_TRUE(IsAtEnd(at_end_segment_stream));

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(at_end_segment_stream);

  ASSERT_TRUE(IsAtEnd(move_assigned_segment_stream));
}

TEST(SegmentStreamTest,
     MoveAssignmentOperatorShouldUnsetIsAtEndWhenRhsIsNotAtEnd) {
  SegmentStream not_at_end_segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(not_at_end_segment_stream));

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(not_at_end_segment_stream);

  ASSERT_FALSE(IsAtEnd(move_assigned_segment_stream));
}

TEST(SegmentStreamTest, MoveAssignmentOperatorShouldCopyRhsPosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  segment_stream.seek(kInsideBufferPosition);
  ASSERT_EQ(kInsideBufferPosition, segment_stream.getPosition());

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(segment_stream);

  ASSERT_EQ(kInsideBufferPosition, move_assigned_segment_stream.getPosition());
}

TEST(SegmentStreamTest, MoveAssignmentOperatorShouldCopyRhsLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());

  SegmentStream move_assigned_segment_stream;
  move_assigned_segment_stream = std::move(segment_stream);

  ASSERT_EQ(kBufferAllocationSize, move_assigned_segment_stream.getLength());
}

TEST(SegmentStreamTest, SetReaderShouldUnsetIsCleared) {
  SegmentStream segment_stream;
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();
  ASSERT_TRUE(IsCleared(segment_stream));

  segment_stream.SetReader(segment_reader);

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldUnsetIsAtEnd) {
  SegmentStream segment_stream;
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();
  ASSERT_TRUE(IsAtEnd(segment_stream));

  segment_stream.SetReader(segment_reader);

  ASSERT_FALSE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldNotChangePosition) {
  SegmentStream segment_stream;
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  segment_stream.SetReader(segment_reader);

  ASSERT_TRUE(PositionIsZero(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldUpdateLength) {
  SegmentStream segment_stream;
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();
  ASSERT_FALSE(LengthIsAllocationSize(segment_stream));

  segment_stream.SetReader(segment_reader);

  ASSERT_TRUE(LengthIsAllocationSize(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldSetIsClearedWhenSetToNull) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsCleared(segment_stream));

  segment_stream.SetReader(nullptr);

  ASSERT_TRUE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldSetIsClearedWhenReaderSizeNotBigEnough) {
  SegmentStream segment_stream;
  segment_stream.seek(kPastEndOfBufferPosition);
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();

  segment_stream.SetReader(segment_reader);

  ASSERT_TRUE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldSetIsAtEndWhenReaderSizeNotBigEnough) {
  SegmentStream segment_stream;
  segment_stream.seek(kPastEndOfBufferPosition);
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();

  segment_stream.SetReader(segment_reader);

  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest,
     SetReaderShouldNotChangePositionWhenReaderSizeNotBigEnough) {
  SegmentStream segment_stream;
  segment_stream.seek(kPastEndOfBufferPosition);
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();

  segment_stream.SetReader(segment_reader);

  ASSERT_EQ(kPastEndOfBufferPosition, segment_stream.getPosition());
}

TEST(SegmentStreamTest, SetReaderShouldChangeLengthWhenReaderSizeNotBigEnough) {
  SegmentStream segment_stream;
  segment_stream.seek(kPastEndOfBufferPosition);
  scoped_refptr<SegmentReader> segment_reader = CreateSegmentReader();

  segment_stream.SetReader(segment_reader);

  ASSERT_TRUE(LengthIsAllocationSize(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldSetIsAtEndWhenSetToNull) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  segment_stream.SetReader(nullptr);
  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, SetReaderShouldNotChangePositionWhenSetToNull) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  const size_t amount_read = ReadFromSegmentStream(segment_stream);
  ASSERT_EQ(kInsideBufferPosition, amount_read);
  const size_t pre_nulled_position = segment_stream.getPosition();
  ASSERT_EQ(kInsideBufferPosition, pre_nulled_position);

  segment_stream.SetReader(nullptr);

  ASSERT_EQ(kInsideBufferPosition, segment_stream.getPosition());
}

TEST(SegmentStreamTest, SetReaderShouldClearLengthWhenSetToNull) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(LengthIsZero(segment_stream));

  segment_stream.SetReader(nullptr);

  ASSERT_TRUE(LengthIsZero(segment_stream));
}

TEST(SegmentStreamTest, ReadShouldConsumeBuffer) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  const size_t amount_read = ReadFromSegmentStream(segment_stream);

  ASSERT_EQ(kInsideBufferPosition, amount_read);
}

TEST(SegmentStreamTest, ReadShouldNotClear) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  ReadFromSegmentStream(segment_stream);

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, ReadShouldUpdateIsAtEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  ReadFromSegmentStream(segment_stream, kBufferAllocationSize);

  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, ReadShouldUpdatePosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  ReadFromSegmentStream(segment_stream);

  ASSERT_TRUE(PositionIsInsideBuffer(segment_stream));
}

TEST(SegmentStreamTest, ReadShouldNotChangeLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());

  ReadFromSegmentStream(segment_stream);

  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());
}

TEST(SegmentStreamTest, ReadShouldConsumeBufferWithoutGoingPastTheEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  const size_t amount_read =
      ReadFromSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_EQ(kBufferAllocationSize, amount_read);
}

TEST(SegmentStreamTest, ReadShouldSetIsAtEndWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  ReadFromSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, ReadShouldTruncatePositionWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  ReadFromSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_TRUE(PositionIsAtEndOfBuffer(segment_stream));
}

TEST(SegmentStreamTest, PeekShouldConsumeBuffer) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  const size_t amount_peeked = PeekIntoSegmentStream(segment_stream);

  ASSERT_EQ(kInsideBufferPosition, amount_peeked);
}

TEST(SegmentStreamTest, PeekShouldNotClear) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  PeekIntoSegmentStream(segment_stream);

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, PeekShouldNotUpdateIsAtEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  PeekIntoSegmentStream(segment_stream, kBufferAllocationSize);

  ASSERT_FALSE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, PeekShouldNotUpdatePosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  PeekIntoSegmentStream(segment_stream);

  ASSERT_TRUE(PositionIsZero(segment_stream));
}

TEST(SegmentStreamTest, PeekShouldNotChangeLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  PeekIntoSegmentStream(segment_stream);

  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());
}

TEST(SegmentStreamTest, PeekShouldConsumeBufferWithoutGoingPastTheEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  const size_t amount_peeked =
      PeekIntoSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_EQ(kBufferAllocationSize, amount_peeked);
}

TEST(SegmentStreamTest, PeekShouldNotSetIsAtEndWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  PeekIntoSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_FALSE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, PeekShouldNotTruncatePositionWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  PeekIntoSegmentStream(segment_stream, kPastEndOfBufferPosition);

  ASSERT_TRUE(PositionIsZero(segment_stream));
}

TEST(SegmentStreamTest, RewindShouldNotClear) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ReadFromSegmentStream(segment_stream);
  ASSERT_FALSE(IsCleared(segment_stream));

  segment_stream.rewind();

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, RewindShouldNotSetAtEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ReadFromSegmentStream(segment_stream);
  ASSERT_FALSE(IsAtEnd(segment_stream));

  segment_stream.rewind();

  ASSERT_FALSE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, RewindShouldResetPosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ReadFromSegmentStream(segment_stream);
  ASSERT_TRUE(PositionIsInsideBuffer(segment_stream));

  segment_stream.rewind();

  ASSERT_TRUE(PositionIsZero(segment_stream));
}

TEST(SegmentStreamTest, RewindShouldNotChangeLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ReadFromSegmentStream(segment_stream);

  segment_stream.rewind();

  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());
}

TEST(SegmentStreamTest, HasPositionShouldBeSupported) {
  SegmentStream segment_stream;

  ASSERT_TRUE(segment_stream.hasPosition());
}

TEST(SegmentStreamTest, SeekShouldNotSetIsCleared) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsCleared(segment_stream));

  segment_stream.seek(kInsideBufferPosition);

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, SeekShouldNotSetIsAtEndWhenSeekingInsideTheBuffer) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  segment_stream.seek(kInsideBufferPosition);

  ASSERT_FALSE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, SeekShouldSetIsAtEndWhenSeekingToTheEndOfTheBuffer) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_FALSE(IsAtEnd(segment_stream));

  segment_stream.seek(kBufferAllocationSize);

  ASSERT_TRUE(IsAtEnd(segment_stream));
}

TEST(SegmentStreamTest, SeekShouldUpdatePosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  segment_stream.seek(kInsideBufferPosition);

  ASSERT_EQ(kInsideBufferPosition, segment_stream.getPosition());
}

TEST(SegmentStreamTest, SeekShouldNotTruncatePositionWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  segment_stream.seek(kPastEndOfBufferPosition);

  ASSERT_EQ(kPastEndOfBufferPosition, segment_stream.getPosition());
}

TEST(SegmentStreamTest, SeekShouldNotUpdateLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  segment_stream.seek(kInsideBufferPosition);

  ASSERT_EQ(kBufferAllocationSize, segment_stream.getLength());
}

TEST(SegmentStreamTest, MoveShouldNotSetCleared) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();

  segment_stream.move(kInsideBufferPosition);

  ASSERT_FALSE(IsCleared(segment_stream));
}

TEST(SegmentStreamTest, MoveShouldUpdatePosition) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  segment_stream.move(kInsideBufferPosition);

  ASSERT_TRUE(PositionIsInsideBuffer(segment_stream));
}

TEST(SegmentStreamTest, MoveShouldNotTruncatePositionWhenPastEnd) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(PositionIsZero(segment_stream));

  segment_stream.move(kPastEndOfBufferPosition);

  ASSERT_EQ(kPastEndOfBufferPosition, segment_stream.getPosition());
}

TEST(SegmentStreamTest, MoveShouldNotChangeLength) {
  SegmentStream segment_stream = CreatePopulatedSegmentStream();
  ASSERT_TRUE(LengthIsAllocationSize(segment_stream));

  segment_stream.move(kInsideBufferPosition);

  ASSERT_TRUE(LengthIsAllocationSize(segment_stream));
}

TEST(SegmentStreamTest, HasLengthShouldBeSupported) {
  SegmentStream segment_stream;
  ASSERT_TRUE(segment_stream.hasLength());
}

namespace {

::testing::AssertionResult IsCleared(const SegmentStream& segment_stream) {
  if (segment_stream.IsCleared())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "SegmentStream is not clear";
}

::testing::AssertionResult IsAtEnd(const SegmentStream& segment_stream) {
  if (segment_stream.isAtEnd())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "SegmentStream is not at the end";
}

::testing::AssertionResult PositionIsZero(const SegmentStream& segment_stream) {
  if (segment_stream.getPosition() == 0ul)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "SegmentStream position is not 0";
}

::testing::AssertionResult PositionIsInsideBuffer(
    const SegmentStream& segment_stream) {
  if (segment_stream.getPosition() == kInsideBufferPosition)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "SegmentStream position is not inside the buffer";
}

::testing::AssertionResult PositionIsAtEndOfBuffer(
    const SegmentStream& segment_stream) {
  if (segment_stream.getPosition() == kBufferAllocationSize)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "SegmentStream position is not at the end of the buffer";
}

::testing::AssertionResult LengthIsZero(const SegmentStream& segment_stream) {
  if (segment_stream.getLength() == 0ul)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "SegmentStream length is not 0";
}

::testing::AssertionResult LengthIsAllocationSize(
    const SegmentStream& segment_stream) {
  if (segment_stream.getLength() == kBufferAllocationSize)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "SegmentStream length is not the allocation size";
}

SegmentStream CreatePopulatedSegmentStream() {
  SegmentStream segment_stream;
  segment_stream.SetReader(CreateSegmentReader());
  return segment_stream;
}

scoped_refptr<SegmentReader> CreateSegmentReader() {
  Vector<char> raw_buffer(kBufferAllocationSize);

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(raw_buffer.data(), kBufferAllocationSize);

  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSharedBuffer(std::move(shared_buffer));

  return segment_reader;
}

size_t ReadFromSegmentStream(SegmentStream& segment_stream,
                             size_t amount_to_read) {
  Vector<char> read_buffer(kBufferAllocationSize);
  return segment_stream.read(read_buffer.data(), amount_to_read);
}

size_t PeekIntoSegmentStream(SegmentStream& segment_stream,
                             size_t amount_to_peek) {
  Vector<char> peek_buffer(kBufferAllocationSize);
  return segment_stream.peek(peek_buffer.data(), amount_to_peek);
}

}  // namespace
}  // namespace blink
