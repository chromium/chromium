// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/media_session/public/cpp/media_session_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::MediaImageBitmap;

namespace media_session {

using MojoTraitsTest = testing::Test;

TEST_F(MojoTraitsTest, ColorTypeConversion_RGBA_8888) {
  SkBitmap input;
  SkImageInfo info =
      SkImageInfo::Make(200, 100, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  input.allocPixels(info);

  SkBitmap output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<MediaImageBitmap>(&input, &output));

  // The output image should have the same properties but should have the
  // color type standardised on the platform default.
  EXPECT_FALSE(output.isNull());
  EXPECT_EQ(kN32_SkColorType, output.info().colorType());
  EXPECT_EQ(200, output.info().width());
  EXPECT_EQ(100, output.info().height());
  EXPECT_EQ(kPremul_SkAlphaType, output.info().alphaType());
}

TEST_F(MojoTraitsTest, ColorTypeConversion_BGRA_8888) {
  SkBitmap input;
  SkImageInfo info =
      SkImageInfo::Make(200, 100, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  input.allocPixels(info);

  SkBitmap output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<MediaImageBitmap>(&input, &output));

  // The output image should have the same properties but should have the
  // color type standardised on the platform default.
  EXPECT_FALSE(output.isNull());
  EXPECT_EQ(kN32_SkColorType, output.info().colorType());
  EXPECT_EQ(200, output.info().width());
  EXPECT_EQ(100, output.info().height());
  EXPECT_EQ(kPremul_SkAlphaType, output.info().alphaType());
}

}  // namespace media_session
