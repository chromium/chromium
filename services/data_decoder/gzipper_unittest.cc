// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/gzipper.h"

#include <optional>

#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {

void CopyResultCallback(std::optional<mojo_base::BigBuffer>& output_result,
                        std::optional<mojo_base::BigBuffer> result) {
  output_result = std::move(result);
}

using GzipperTest = testing::Test;

}  // namespace

TEST_F(GzipperTest, DeflateAndInflate) {
  Gzipper gzipper;
  std::vector<uint8_t> input = {0x01, 0x01, 0x01, 0x02, 0x02, 0x02};
  std::optional<mojo_base::BigBuffer> compressed;
  gzipper.Deflate(input,
                  base::BindOnce(&CopyResultCallback, std::ref(compressed)));
  ASSERT_TRUE(compressed.has_value());
  EXPECT_THAT(base::make_span(*compressed),
              testing::Not(testing::ElementsAreArray(base::make_span(input))));

  std::optional<mojo_base::BigBuffer> uncompressed;
  gzipper.Inflate(std::move(*compressed), input.size(),
                  base::BindOnce(&CopyResultCallback, std::ref(uncompressed)));
  ASSERT_TRUE(uncompressed.has_value());
  EXPECT_THAT(base::make_span(*uncompressed),
              testing::ElementsAreArray(base::make_span(input)));
}

// Test not allocating enough space to inflate data.
TEST_F(GzipperTest, InflateExceedsSize) {
  Gzipper gzipper;
  std::vector<uint8_t> input = {0x01, 0x01, 0x01, 0x02, 0x02, 0x02};
  std::optional<mojo_base::BigBuffer> compressed;
  gzipper.Deflate(input,
                  base::BindOnce(&CopyResultCallback, std::ref(compressed)));
  ASSERT_TRUE(compressed.has_value());
  std::optional<mojo_base::BigBuffer> uncompressed;
  gzipper.Inflate(std::move(*compressed), input.size() - 1,
                  base::BindOnce(&CopyResultCallback, std::ref(uncompressed)));
  EXPECT_FALSE(uncompressed.has_value());
}

// Test allocating more than the space needed to inflate data.
TEST_F(GzipperTest, InflateTrimsSize) {
  Gzipper gzipper;
  std::vector<uint8_t> input = {0x01, 0x01, 0x01, 0x02, 0x02, 0x02};
  std::optional<mojo_base::BigBuffer> compressed;
  gzipper.Deflate(input,
                  base::BindOnce(&CopyResultCallback, std::ref(compressed)));
  ASSERT_TRUE(compressed.has_value());
  std::optional<mojo_base::BigBuffer> uncompressed;
  gzipper.Inflate(std::move(*compressed), input.size() + 1,
                  base::BindOnce(&CopyResultCallback, std::ref(uncompressed)));
  ASSERT_TRUE(uncompressed.has_value());
  EXPECT_THAT(base::make_span(*uncompressed),
              testing::ElementsAreArray(base::make_span(input)));
}

TEST_F(GzipperTest, CompressAndUncompress) {
  Gzipper gzipper;
  std::vector<uint8_t> input = {0x01, 0x01, 0x01, 0x02, 0x02, 0x02};
  std::optional<mojo_base::BigBuffer> compressed;
  gzipper.Compress(input,
                   base::BindOnce(&CopyResultCallback, std::ref(compressed)));
  ASSERT_TRUE(compressed.has_value());
  EXPECT_THAT(base::make_span(*compressed),
              testing::Not(testing::ElementsAreArray(base::make_span(input))));

  std::optional<mojo_base::BigBuffer> uncompressed;
  gzipper.Uncompress(
      std::move(*compressed),
      base::BindOnce(&CopyResultCallback, std::ref(uncompressed)));
  ASSERT_TRUE(uncompressed.has_value());
  EXPECT_THAT(base::make_span(*uncompressed),
              testing::ElementsAreArray(base::make_span(input)));
}

}  // namespace data_decoder
