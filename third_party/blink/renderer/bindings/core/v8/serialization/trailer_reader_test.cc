// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_reader.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"

using ::testing::UnorderedElementsAre;

namespace blink {
namespace {

MATCHER(FoundTrailer, "") {
  return arg.has_value() && arg.value();
}
MATCHER(FoundNoTrailer, "") {
  return arg.has_value() && !arg.value();
}
MATCHER(SawInvalidHeader, "") {
  return !arg.has_value() &&
         arg.error() == TrailerReader::Error::kInvalidHeader;
}

MATCHER(Succeeded, "") {
  return arg.has_value();
}
MATCHER(SawInvalidTrailer, "") {
  return !arg.has_value() &&
         arg.error() == TrailerReader::Error::kInvalidTrailer;
}

TEST(TrailerReaderTest, SkipToTrailer_Empty) {
  TrailerReader reader({});
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_NoVersion) {
  constexpr uint8_t kData[] = {'0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), FoundNoTrailer());
}

TEST(TrailerReaderTest, SkipToTrailer_VersionTooLow) {
  constexpr uint8_t kData[] = {0xff, 0x09, '0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), FoundNoTrailer());
}

TEST(TrailerReaderTest, SkipToTrailer_VersionTooHigh) {
  constexpr uint8_t kData[] = {0xff, 0xff, 0xff, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_VersionOverflow) {
  constexpr uint8_t kData[] = {0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_NoTrailerTag) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xff, 0x0f, '0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_TruncatedOffset) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_TruncatedSize) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_NoTrailer) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0xff, 0x0f, '0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), FoundNoTrailer());
}

TEST(TrailerReaderTest, SkipToTrailer_OffsetTooSmall) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x01, 0xff, 0x0f, '0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_OffsetTooLarge) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                               0x00, 0x00, 0x10, 0xff, 0x0f, '0'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_SizeTooLarge) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0xff,
                               0x0f, '0',  't',  'e',  's',  't'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), SawInvalidHeader());
}

TEST(TrailerReaderTest, SkipToTrailer_ValidRange) {
  constexpr uint8_t kData[] = {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x04, 0xff,
                               0x0f, '0',  't',  'e',  's',  't'};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.SkipToTrailer(), FoundTrailer());
  EXPECT_EQ(reader.GetPositionForTesting(), 18u);
}

TEST(TrailerReaderTest, Read_Empty) {
  TrailerReader reader({});
  EXPECT_THAT(reader.Read(), Succeeded());
  EXPECT_THAT(reader.required_exposed_interfaces(), ::testing::IsEmpty());
}

TEST(TrailerReaderTest, Read_UnrecognizedTrailerTag) {
  constexpr uint8_t kData[] = {0x32, 0x00, 0x00, 0x00, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.Read(), SawInvalidTrailer());
}

TEST(TrailerReaderTest, Read_TruncatedInterfaceCount) {
  constexpr uint8_t kData[] = {0x32, 0x00, 0x00, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.Read(), SawInvalidTrailer());
}

TEST(TrailerReaderTest, Read_TruncatedExposedInterfaces) {
  constexpr uint8_t kData[] = {0xa0, 0x00, 0x00, 0x00, 0x02, kImageBitmapTag};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.Read(), SawInvalidTrailer());
}

TEST(TrailerReaderTest, Read_ZeroInterfaceCount) {
  constexpr uint8_t kData[] = {0xa0, 0x00, 0x00, 0x00, 0x00};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.Read(), Succeeded());
  EXPECT_THAT(reader.required_exposed_interfaces(), ::testing::IsEmpty());
}

TEST(TrailerReaderTest, Read_ValidExposedInterfaces) {
  constexpr uint8_t kData[] = {
      0xa0, 0x00, 0x00, 0x00, 0x02, kImageBitmapTag, kCryptoKeyTag};
  TrailerReader reader(kData);
  EXPECT_THAT(reader.Read(), Succeeded());
  EXPECT_THAT(reader.required_exposed_interfaces(),
              UnorderedElementsAre(kImageBitmapTag, kCryptoKeyTag));
}

TEST(TrailerReaderTest, Read_AfterSkipToTrailer) {
  constexpr uint8_t kData[] = {
      0xff,         0x15, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,         0x00, 0x12, 0x00, 0x00, 0x00, 0x07, 0xff,
      0x0f,         '0',  0xa0, 0x00, 0x00, 0x00, 0x02, kImageBitmapTag,
      kCryptoKeyTag};
  TrailerReader reader(kData);
  ASSERT_THAT(reader.SkipToTrailer(), FoundTrailer());
  EXPECT_EQ(reader.GetPositionForTesting(), 18u);
  ASSERT_THAT(reader.Read(), Succeeded());
  EXPECT_THAT(reader.required_exposed_interfaces(),
              UnorderedElementsAre(kImageBitmapTag, kCryptoKeyTag));
}

TEST(TrailerReaderTest, Read_AfterSkipToTrailer_SizeTooSmall) {
  constexpr uint8_t kData[] = {
      0xff,         0x15, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,         0x00, 0x12, 0x00, 0x00, 0x00, 0x05, 0xff,
      0x0f,         '0',  0xa0, 0x00, 0x00, 0x00, 0x02, kImageBitmapTag,
      kCryptoKeyTag};
  TrailerReader reader(kData);
  ASSERT_THAT(reader.SkipToTrailer(), FoundTrailer());
  EXPECT_EQ(reader.GetPositionForTesting(), 18u);
  ASSERT_THAT(reader.Read(), SawInvalidTrailer());
}

}  // namespace
}  // namespace blink
