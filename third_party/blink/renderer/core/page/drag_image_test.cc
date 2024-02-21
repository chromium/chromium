/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/page/drag_image.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class TestImage : public Image {
 public:
  static scoped_refptr<TestImage> Create(sk_sp<SkImage> image) {
    return base::AdoptRef(new TestImage(image));
  }

  static scoped_refptr<TestImage> Create(const gfx::Size& size) {
    return base::AdoptRef(new TestImage(size));
  }

  gfx::Size SizeWithConfig(SizeConfig) const override {
    DCHECK(image_);
    return gfx::Size(image_->width(), image_->height());
  }

  bool CurrentFrameKnownToBeOpaque() override { return false; }

  void DestroyDecodedData() override {
    // Image pure virtual stub.
  }

  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const gfx::RectF& dest_rect,
            const gfx::RectF& src_rect,
            const ImageDrawOptions&) override {
    // Image pure virtual stub.
  }

  PaintImage PaintImageForCurrentFrame() override {
    if (!image_)
      return PaintImage();
    return CreatePaintImageBuilder()
        .set_image(image_, cc::PaintImage::GetNextContentId())
        .TakePaintImage();
  }

 private:
  explicit TestImage(sk_sp<SkImage> image) : image_(image) {}

  explicit TestImage(gfx::Size size) : image_(nullptr) {
    sk_sp<SkSurface> surface = CreateSkSurface(size);
    if (!surface)
      return;

    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    image_ = surface->makeImageSnapshot();
  }

  static sk_sp<SkSurface> CreateSkSurface(gfx::Size size) {
    return SkSurfaces::Raster(
        SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType));
  }

  sk_sp<SkImage> image_;
};

TEST(DragImageTest, NullHandling) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(DragImage::Create(nullptr));

  scoped_refptr<TestImage> null_test_image(TestImage::Create(gfx::Size()));
  EXPECT_FALSE(DragImage::Create(null_test_image.get()));
}

TEST(DragImageTest, NonNullHandling) {
  test::TaskEnvironment task_environment;
  scoped_refptr<TestImage> test_image(TestImage::Create(gfx::Size(2, 2)));
  std::unique_ptr<DragImage> drag_image = DragImage::Create(test_image.get());
  ASSERT_TRUE(drag_image);

  drag_image->Scale(0.5, 0.5);
  gfx::Size size = drag_image->Size();
  EXPECT_EQ(1, size.width());
  EXPECT_EQ(1, size.height());
}

TEST(DragImageTest, CreateDragImage) {
  test::TaskEnvironment task_environment;
  // Tests that the DrageImage implementation doesn't choke on null values
  // of imageForCurrentFrame().
  // FIXME: how is this test any different from test NullHandling?
  scoped_refptr<TestImage> test_image(TestImage::Create(gfx::Size()));
  EXPECT_FALSE(DragImage::Create(test_image.get()));
}

TEST(DragImageTest, TrimWhitespace) {
  test::TaskEnvironment task_environment;
  KURL url("http://www.example.com/");
  String test_label = "          Example Example Example      \n    ";
  String expected_label = "Example Example Example";
  float device_scale_factor = 1.0f;

  std::unique_ptr<DragImage> test_image =
      DragImage::Create(url, test_label, device_scale_factor);
  std::unique_ptr<DragImage> expected_image =
      DragImage::Create(url, expected_label, device_scale_factor);

  EXPECT_EQ(test_image->Size().width(), expected_image->Size().width());
}

TEST(DragImageTest, InterpolationNone) {
  test::TaskEnvironment task_environment;
  SkBitmap expected_bitmap;
  expected_bitmap.allocN32Pixels(4, 4);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 2, 2), 0xFFFFFFFF);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(0, 2, 2, 2), 0xFF000000);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(2, 0, 2, 2), 0xFF000000);
  expected_bitmap.eraseArea(SkIRect::MakeXYWH(2, 2, 2, 2), 0xFFFFFFFF);

  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(2, 2);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 1, 1), 0xFFFFFFFF);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(0, 1, 1, 1), 0xFF000000);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(1, 0, 1, 1), 0xFF000000);
  test_bitmap.eraseArea(SkIRect::MakeXYWH(1, 1, 1, 1), 0xFFFFFFFF);

  scoped_refptr<TestImage> test_image =
      TestImage::Create(SkImages::RasterFromBitmap(test_bitmap));
  std::unique_ptr<DragImage> drag_image = DragImage::Create(
      test_image.get(), kRespectImageOrientation, kInterpolationNone);
  ASSERT_TRUE(drag_image);
  drag_image->Scale(2, 2);
  const SkBitmap& drag_bitmap = drag_image->Bitmap();
  for (int x = 0; x < drag_bitmap.width(); ++x)
    for (int y = 0; y < drag_bitmap.height(); ++y)
      EXPECT_EQ(expected_bitmap.getColor(x, y), drag_bitmap.getColor(x, y));
}

}  // namespace blink
