// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_ble_frames.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<uint8_t> GetSomeData(size_t size) {
  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i)
    data[i] = static_cast<uint8_t>((i * i) & 0xFF);
  return data;
}

}  // namespace

namespace device {

TEST(FidoBleFramesTest, InitializationFragment) {
  const std::vector<uint8_t> data = GetSomeData(25);
  constexpr uint16_t kDataLength = 21123;

  FidoBleFrameInitializationFragment fragment(
      FidoBleDeviceCommand::kMsg, kDataLength, base::make_span(data));
  std::vector<uint8_t> buffer;
  const size_t binary_size = fragment.Serialize(&buffer);
  EXPECT_EQ(buffer.size(), binary_size);

  EXPECT_EQ(data.size() + 3, binary_size);

  FidoBleFrameInitializationFragment parsed_fragment;
  ASSERT_TRUE(
      FidoBleFrameInitializationFragment::Parse(buffer, &parsed_fragment));

  EXPECT_EQ(kDataLength, parsed_fragment.data_length());
  EXPECT_TRUE(base::ranges::equal(data, parsed_fragment.fragment()));
  EXPECT_EQ(FidoBleDeviceCommand::kMsg, parsed_fragment.command());
}

TEST(FidoBleFramesTest, ContinuationFragment) {
  const auto data = GetSomeData(25);
  constexpr uint8_t kSequence = 61;

  FidoBleFrameContinuationFragment fragment(base::make_span(data), kSequence);

  std::vector<uint8_t> buffer;
  const size_t binary_size = fragment.Serialize(&buffer);
  EXPECT_EQ(buffer.size(), binary_size);

  EXPECT_EQ(data.size() + 1, binary_size);

  FidoBleFrameContinuationFragment parsed_fragment;
  ASSERT_TRUE(
      FidoBleFrameContinuationFragment::Parse(buffer, &parsed_fragment));

  EXPECT_TRUE(base::ranges::equal(data, parsed_fragment.fragment()));
  EXPECT_EQ(kSequence, parsed_fragment.sequence());
}

TEST(FidoBleFramesTest, SplitAndAssemble) {
  for (size_t size : {0,  1,  16, 17, 18, 20, 21, 22, 35,  36,
                      37, 39, 40, 41, 54, 55, 56, 60, 100, 65535}) {
    SCOPED_TRACE(size);

    FidoBleFrame frame(FidoBleDeviceCommand::kPing, GetSomeData(size));

    auto fragments = frame.ToFragments(20);

    EXPECT_EQ(frame.command(), fragments.first.command());
    EXPECT_EQ(frame.data().size(),
              static_cast<size_t>(fragments.first.data_length()));

    FidoBleFrameAssembler assembler(fragments.first);
    while (!fragments.second.empty()) {
      ASSERT_TRUE(assembler.AddFragment(fragments.second.front()));
      fragments.second.pop();
    }

    EXPECT_TRUE(assembler.IsDone());
    ASSERT_TRUE(assembler.GetFrame());

    auto result_frame = std::move(*assembler.GetFrame());
    EXPECT_EQ(frame.command(), result_frame.command());
    EXPECT_EQ(frame.data(), result_frame.data());
  }
}

TEST(FidoBleFramesTest, FrameAssemblerError) {
  FidoBleFrame frame(FidoBleDeviceCommand::kPing, GetSomeData(30));

  auto fragments = frame.ToFragments(20);
  ASSERT_EQ(1u, fragments.second.size());

  fragments.second.front() =
      FidoBleFrameContinuationFragment(fragments.second.front().fragment(), 51);

  FidoBleFrameAssembler assembler(fragments.first);
  EXPECT_FALSE(assembler.IsDone());
  EXPECT_FALSE(assembler.GetFrame());
  EXPECT_FALSE(assembler.AddFragment(fragments.second.front()));
  EXPECT_FALSE(assembler.IsDone());
  EXPECT_FALSE(assembler.GetFrame());
}

TEST(FidoBleFramesTest, FrameGettersAndValidity) {
  {
    FidoBleFrame frame(FidoBleDeviceCommand::kKeepAlive,
                       std::vector<uint8_t>(2));
    EXPECT_FALSE(frame.IsValid());
  }
  {
    FidoBleFrame frame(FidoBleDeviceCommand::kError, {});
    EXPECT_FALSE(frame.IsValid());
  }

  for (auto code : {FidoBleFrame::KeepaliveCode::TUP_NEEDED,
                    FidoBleFrame::KeepaliveCode::PROCESSING}) {
    FidoBleFrame frame(FidoBleDeviceCommand::kKeepAlive,
                       std::vector<uint8_t>(1, static_cast<uint8_t>(code)));
    EXPECT_TRUE(frame.IsValid());
    EXPECT_EQ(code, frame.GetKeepaliveCode());
  }

  for (auto code : {
           FidoBleFrame::ErrorCode::INVALID_CMD,
           FidoBleFrame::ErrorCode::INVALID_PAR,
           FidoBleFrame::ErrorCode::INVALID_SEQ,
           FidoBleFrame::ErrorCode::INVALID_LEN,
           FidoBleFrame::ErrorCode::REQ_TIMEOUT,
           FidoBleFrame::ErrorCode::NA_1,
           FidoBleFrame::ErrorCode::NA_2,
           FidoBleFrame::ErrorCode::NA_3,
       }) {
    FidoBleFrame frame(FidoBleDeviceCommand::kError,
                       {static_cast<uint8_t>(code)});
    EXPECT_TRUE(frame.IsValid());
    EXPECT_EQ(code, frame.GetErrorCode());
  }
}

}  // namespace device
