/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_image.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

static scoped_refptr<SharedBuffer> ReadFile(const char* file_name) {
  String file_path = test::CoreTestDataPath(file_name);
  std::optional<Vector<char>> data = test::ReadFromFile(file_path);
  CHECK(data);
  return SharedBuffer::Create(std::move(*data));
}

class WebImageTest : public testing::Test, private ScopedMockOverlayScrollbars {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(WebImageTest, PNGImage) {
  scoped_refptr<SharedBuffer> data = ReadFile("white-1x1.png");
  SkBitmap image = WebImage::FromData(WebData(data), gfx::Size());
  EXPECT_EQ(image.width(), 1);
  EXPECT_EQ(image.height(), 1);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), image.getColor(0, 0));
}

TEST_F(WebImageTest, ICOImage) {
  scoped_refptr<SharedBuffer> data = ReadFile("black-and-white.ico");
  WebVector<SkBitmap> images = WebImage::FramesFromData(WebData(data));
  ASSERT_EQ(2u, images.size());
  EXPECT_EQ(images[0].width(), 2);
  EXPECT_EQ(images[0].height(), 2);
  EXPECT_EQ(images[1].width(), 1);
  EXPECT_EQ(images[1].height(), 1);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), images[0].getColor(0, 0));
  EXPECT_EQ(SkColorSetARGB(255, 0, 0, 0), images[1].getColor(0, 0));
}

TEST_F(WebImageTest, ICOValidHeaderMissingBitmap) {
  scoped_refptr<SharedBuffer> data =
      ReadFile("valid_header_missing_bitmap.ico");
  WebVector<SkBitmap> images = WebImage::FramesFromData(WebData(data));
  ASSERT_TRUE(images.empty());
}

TEST_F(WebImageTest, BadImage) {
  const char kBadImage[] = "hello world";
  WebVector<SkBitmap> images = WebImage::FramesFromData(WebData(kBadImage));
  ASSERT_EQ(0u, images.size());

  SkBitmap image = WebImage::FromData(WebData(kBadImage), gfx::Size());
  EXPECT_TRUE(image.empty());
  EXPECT_TRUE(image.isNull());
}

TEST_F(WebImageTest, DecodeSVGDesiredSize) {
  const char kImage[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='32'"
      " height='32'></svg>";
  SkBitmap image = WebImage::DecodeSVG(WebData(kImage), gfx::Size(16, 16));
  EXPECT_FALSE(image.empty());
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(image.width(), 16);
  EXPECT_EQ(image.height(), 16);
}

TEST_F(WebImageTest, DecodeSVGDesiredSizeAspectRatioOnly) {
  const char kImageAspectRatioOne[] =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'></svg>";
  SkBitmap image =
      WebImage::DecodeSVG(WebData(kImageAspectRatioOne), gfx::Size(16, 16));
  EXPECT_FALSE(image.empty());
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(image.width(), 16);
  EXPECT_EQ(image.height(), 16);

  const char kImageAspectRatioNotOne[] =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 3'></svg>";
  image =
      WebImage::DecodeSVG(WebData(kImageAspectRatioNotOne), gfx::Size(16, 16));
  EXPECT_FALSE(image.empty());
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(image.width(), 16);
  EXPECT_EQ(image.height(), 16);
}

TEST_F(WebImageTest, DecodeSVGDesiredSizeEmpty) {
  const char kImage[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='32'"
      " height='32'></svg>";
  SkBitmap image = WebImage::DecodeSVG(WebData(kImage), gfx::Size());
  EXPECT_FALSE(image.empty());
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(image.width(), 32);
  EXPECT_EQ(image.height(), 32);
}

TEST_F(WebImageTest, DecodeSVGInvalidImage) {
  const char kBogusImage[] = "bogus";
  SkBitmap image = WebImage::DecodeSVG(WebData(kBogusImage), gfx::Size(16, 16));
  EXPECT_TRUE(image.empty());
  EXPECT_TRUE(image.isNull());

  const char kWellformedXMLBadImage[] = "<foo xmlns='some:namespace'></foo>";
  image =
      WebImage::DecodeSVG(WebData(kWellformedXMLBadImage), gfx::Size(16, 16));
  EXPECT_TRUE(image.empty());
  EXPECT_TRUE(image.isNull());
}

}  // namespace blink
