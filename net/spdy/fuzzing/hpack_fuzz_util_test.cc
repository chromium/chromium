// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/fuzzing/hpack_fuzz_util.h"

#include <map>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "net/third_party/quiche/src/spdy/platform/api/spdy_string_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spdy {
namespace test {

using std::map;

TEST(HpackFuzzUtilTest, GeneratorContextInitialization) {
  HpackFuzzUtil::GeneratorContext context;
  HpackFuzzUtil::InitializeGeneratorContext(&context);

  // Context was seeded with initial name & value fixtures.
  EXPECT_LT(0u, context.names.size());
  EXPECT_LT(0u, context.values.size());
}

TEST(HpackFuzzUtil, GeneratorContextExpansion) {
  HpackFuzzUtil::GeneratorContext context;

  SpdyHeaderBlock headers = HpackFuzzUtil::NextGeneratedHeaderSet(&context);

  // Headers were generated, and the generator context was expanded.
  EXPECT_LT(0u, headers.size());
  EXPECT_LT(0u, context.names.size());
  EXPECT_LT(0u, context.values.size());
}

// TODO(jgraettinger): A better test would mock a random generator and
// evaluate SampleExponential along fixed points of the [0,1] domain.
TEST(HpackFuzzUtilTest, SampleExponentialRegression) {
  // TODO(jgraettinger): Upstream uses a seeded random generator here to pin
  // the behavior of SampleExponential. Chromium's random generation utilities
  // are strongly secure, but provide no way to seed the generator.
  for (size_t i = 0; i != 100; ++i) {
    EXPECT_GE(30u, HpackFuzzUtil::SampleExponential(10, 30));
  }
}

TEST(HpackFuzzUtilTest, ParsesSequenceOfHeaderBlocks) {
  char fixture[] =
      "\x00\x00\x00\x05"
      "aaaaa"
      "\x00\x00\x00\x04"
      "bbbb"
      "\x00\x00\x00\x03"
      "ccc"
      "\x00\x00\x00\x02"
      "dd"
      "\x00\x00\x00\x01"
      "e"
      "\x00\x00\x00\x00"
      ""
      "\x00\x00\x00\x03"
      "fin";

  HpackFuzzUtil::Input input;
  input.input.assign(fixture, base::size(fixture) - 1);

  SpdyStringPiece block;

  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("aaaaa", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("bbbb", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("ccc", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("dd", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("e", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("", block);
  EXPECT_TRUE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
  EXPECT_EQ("fin", block);
  EXPECT_FALSE(HpackFuzzUtil::NextHeaderBlock(&input, &block));
}

TEST(HpackFuzzUtilTest, SerializedHeaderBlockPrefixes) {
  EXPECT_EQ(std::string("\x00\x00\x00\x00", 4),
            HpackFuzzUtil::HeaderBlockPrefix(0));
  EXPECT_EQ(std::string("\x00\x00\x00\x05", 4),
            HpackFuzzUtil::HeaderBlockPrefix(5));
  EXPECT_EQ("\x4f\xb3\x0a\x91", HpackFuzzUtil::HeaderBlockPrefix(1337133713));
}

TEST(HpackFuzzUtilTest, PassValidInputThroughAllStages) {
  // Example lifted from HpackDecoderTest.SectionD4RequestHuffmanExamples.
  std::string input = SpdyHexDecode("828684418cf1e3c2e5f23a6ba0ab90f4ff");

  HpackFuzzUtil::FuzzerContext context;
  HpackFuzzUtil::InitializeFuzzerContext(&context);

  EXPECT_TRUE(
      HpackFuzzUtil::RunHeaderBlockThroughFuzzerStages(&context, input));

  SpdyHeaderBlock expect;
  expect[":method"] = "GET";
  expect[":scheme"] = "http";
  expect[":path"] = "/";
  expect[":authority"] = "www.example.com";
  EXPECT_EQ(expect, context.third_stage->decoded_block());
}

TEST(HpackFuzzUtilTest, ValidFuzzExamplesRegressionTest) {
  base::FilePath source_root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));

  // Load the example fixtures versioned with the source tree.
  HpackFuzzUtil::Input input;
  ASSERT_TRUE(base::ReadFileToString(
      source_root.Append(FILE_PATH_LITERAL("net"))
          .Append(FILE_PATH_LITERAL("data"))
          .Append(FILE_PATH_LITERAL("spdy_tests"))
          .Append(FILE_PATH_LITERAL("examples_07.hpack")),
      &input.input));

  HpackFuzzUtil::FuzzerContext context;
  HpackFuzzUtil::InitializeFuzzerContext(&context);

  SpdyStringPiece block;
  while (HpackFuzzUtil::NextHeaderBlock(&input, &block)) {
    // As these are valid examples, all fuzz stages should succeed.
    EXPECT_TRUE(
        HpackFuzzUtil::RunHeaderBlockThroughFuzzerStages(&context, block));
  }
}

TEST(HpackFuzzUtilTest, FlipBitsMutatesBuffer) {
  char buffer[] = "testbuffer1234567890";
  std::string unmodified(buffer, base::size(buffer) - 1);

  EXPECT_EQ(unmodified, buffer);
  HpackFuzzUtil::FlipBits(reinterpret_cast<uint8_t*>(buffer),
                          base::size(buffer) - 1, 1);
  EXPECT_NE(unmodified, buffer);
}

}  // namespace test
}  // namespace spdy
