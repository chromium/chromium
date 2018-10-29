/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"

#include "SkPixelRef.h"  // FIXME: qualify this skia header file.

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ExceptionState;

class ImageBitmapTest : public testing::Test {
 protected:
  void SetUp() override {
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 10);
    surface->getCanvas()->clear(0xFFFFFFFF);
    image_ = surface->makeImageSnapshot();

    sk_sp<SkSurface> surface2 = SkSurface::MakeRasterN32Premul(5, 5);
    surface2->getCanvas()->clear(0xAAAAAAAA);
    image2_ = surface2->makeImageSnapshot();

    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ = ReplaceMemoryCacheForTesting(MemoryCache::Create(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting()));

    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl, nullptr);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }
  void TearDown() override {
    // Garbage collection is required prior to switching out the
    // test's memory cache; image resources are released, evicting
    // them from the cache.
    ThreadState::Current()->CollectGarbage(
        BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
        BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGC);

    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
    SharedGpuContext::ResetForTesting();
  }

 protected:
  FakeGLES2Interface gl_;
  sk_sp<SkImage> image_, image2_;
  Persistent<MemoryCache> global_memory_cache_;
};

TEST_F(ImageBitmapTest, ImageResourceConsistency) {
  const ImageBitmapOptions default_options;
  HTMLImageElement* image_element =
      HTMLImageElement::Create(*Document::CreateForTest());
  sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(5, 5, src_rgb_color_space);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  ImageResourceContent* original_image_resource =
      ImageResourceContent::CreateLoaded(
          StaticBitmapImage::Create(image).get());
  image_element->SetImageForTest(original_image_resource);

  base::Optional<IntRect> crop_rect =
      IntRect(0, 0, image_->width(), image_->height());
  ImageBitmap* image_bitmap_no_crop =
      ImageBitmap::Create(image_element, crop_rect,
                          &(image_element->GetDocument()), default_options);
  ASSERT_TRUE(image_bitmap_no_crop);
  crop_rect = IntRect(image_->width() / 2, image_->height() / 2,
                      image_->width() / 2, image_->height() / 2);
  ImageBitmap* image_bitmap_interior_crop =
      ImageBitmap::Create(image_element, crop_rect,
                          &(image_element->GetDocument()), default_options);
  ASSERT_TRUE(image_bitmap_interior_crop);
  crop_rect = IntRect(-image_->width() / 2, -image_->height() / 2,
                      image_->width(), image_->height());
  ImageBitmap* image_bitmap_exterior_crop =
      ImageBitmap::Create(image_element, crop_rect,
                          &(image_element->GetDocument()), default_options);
  ASSERT_TRUE(image_bitmap_exterior_crop);
  crop_rect = IntRect(-image_->width(), -image_->height(), image_->width(),
                      image_->height());
  ImageBitmap* image_bitmap_outside_crop =
      ImageBitmap::Create(image_element, crop_rect,
                          &(image_element->GetDocument()), default_options);
  ASSERT_TRUE(image_bitmap_outside_crop);

  ASSERT_EQ(image_bitmap_no_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage());
  ASSERT_NE(image_bitmap_interior_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage());
  ASSERT_EQ(image_bitmap_exterior_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage());

  scoped_refptr<StaticBitmapImage> empty_image =
      image_bitmap_outside_crop->BitmapImage();
  ASSERT_NE(empty_image->PaintImageForCurrentFrame().GetSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSkImage());
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a
// reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged) {
  HTMLImageElement* image =
      HTMLImageElement::Create(*Document::CreateForTest());
  sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(5, 5, src_rgb_color_space);
  sk_sp<SkSurface> raster_surface(SkSurface::MakeRaster(raster_image_info));
  sk_sp<SkImage> raster_image = raster_surface->makeImageSnapshot();
  ImageResourceContent* original_image_resource =
      ImageResourceContent::CreateLoaded(
          StaticBitmapImage::Create(raster_image).get());
  image->SetImageForTest(original_image_resource);

  const ImageBitmapOptions default_options;
  base::Optional<IntRect> crop_rect =
      IntRect(0, 0, image_->width(), image_->height());
  ImageBitmap* image_bitmap = ImageBitmap::Create(
      image, crop_rect, &(image->GetDocument()), default_options);
  ASSERT_TRUE(image_bitmap);
  ASSERT_EQ(
      image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage(),
      original_image_resource->GetImage()
          ->PaintImageForCurrentFrame()
          .GetSkImage());

  ImageResourceContent* new_image_resource = ImageResourceContent::CreateLoaded(
      StaticBitmapImage::Create(image2_).get());
  image->SetImageForTest(new_image_resource);

  {
    ASSERT_EQ(
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage(),
        original_image_resource->GetImage()
            ->PaintImageForCurrentFrame()
            .GetSkImage());
    SkImage* image1 = image_bitmap->BitmapImage()
                          ->PaintImageForCurrentFrame()
                          .GetSkImage()
                          .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 = original_image_resource->GetImage()
                          ->PaintImageForCurrentFrame()
                          .GetSkImage()
                          .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_EQ(image1, image2);
  }

  {
    ASSERT_NE(
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage(),
        new_image_resource->GetImage()
            ->PaintImageForCurrentFrame()
            .GetSkImage());
    SkImage* image1 = image_bitmap->BitmapImage()
                          ->PaintImageForCurrentFrame()
                          .GetSkImage()
                          .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 = new_image_resource->GetImage()
                          ->PaintImageForCurrentFrame()
                          .GetSkImage()
                          .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_NE(image1, image2);
  }
}

static void TestImageBitmapTextureBacked(
    scoped_refptr<StaticBitmapImage> bitmap,
    IntRect& rect,
    ImageBitmapOptions options,
    bool is_texture_backed) {
  ImageBitmap* image_bitmap = ImageBitmap::Create(bitmap, rect, options);
  EXPECT_TRUE(image_bitmap);
  EXPECT_EQ(image_bitmap->BitmapImage()->IsTextureBacked(), is_texture_backed);
}

TEST_F(ImageBitmapTest, AvoidGPUReadback) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(100, 100);

  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, imageInfo);
  sk_sp<SkImage> image = surface->makeImageSnapshot();

  scoped_refptr<AcceleratedStaticBitmapImage> bitmap =
      AcceleratedStaticBitmapImage::CreateFromSkImage(image,
                                                      context_provider_wrapper);
  EXPECT_TRUE(bitmap->TextureHolderForTesting()->IsSkiaTextureHolder());

  ImageBitmap* image_bitmap = ImageBitmap::Create(bitmap);
  EXPECT_TRUE(image_bitmap);
  EXPECT_TRUE(image_bitmap->BitmapImage()->IsTextureBacked());

  IntRect image_bitmap_rect(25, 25, 50, 50);
  ImageBitmapOptions image_bitmap_options;
  TestImageBitmapTextureBacked(bitmap, image_bitmap_rect, image_bitmap_options,
                               true);

  std::list<String> image_orientations = {"none", "flipY"};
  std::list<String> premultiply_alphas = {"none", "premultiply", "default"};
  std::list<String> color_space_conversions = {
      "none", "default", "preserve", "srgb", "linear-rgb", "rec2020", "p3"};
  std::list<int> resize_widths = {25, 50, 75};
  std::list<int> resize_heights = {25, 50, 75};
  std::list<String> resize_qualities = {"pixelated", "low", "medium", "high"};

  for (auto image_orientation : image_orientations) {
    for (auto premultiply_alpha : premultiply_alphas) {
      for (auto color_space_conversion : color_space_conversions) {
        for (auto resize_width : resize_widths) {
          for (auto resize_height : resize_heights) {
            for (auto resize_quality : resize_qualities) {
              ImageBitmapOptions image_bitmap_options;
              image_bitmap_options.setImageOrientation(image_orientation);
              image_bitmap_options.setPremultiplyAlpha(premultiply_alpha);
              image_bitmap_options.setColorSpaceConversion(
                  color_space_conversion);
              image_bitmap_options.setResizeWidth(resize_width);
              image_bitmap_options.setResizeHeight(resize_height);
              image_bitmap_options.setResizeQuality(resize_quality);
              // Setting premuliply_alpha to none will cause a read back.
              // Otherwise, we expect to avoid GPU readback when creaing an
              // ImageBitmap from a texture-backed source.
              TestImageBitmapTextureBacked(bitmap, image_bitmap_rect,
                                           image_bitmap_options,
                                           premultiply_alpha != "none");
            }
          }
        }
      }
    }
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionHTMLImageElement) {
  HTMLImageElement* image_element =
      HTMLImageElement::Create(*Document::CreateForTest());

  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> colorspin =
      ColorCorrectionTestUtils::ColorSpinSkColorSpace();

  SkImageInfo image_info = SkImageInfo::MakeN32Premul(10, 10, colorspin);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(image_info));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> source_image = surface->makeImageSnapshot();
  SkPixmap source_pixmap;
  source_image->peekPixels(&source_pixmap);

  ImageResourceContent* original_image_resource =
      ImageResourceContent::CreateLoaded(
          StaticBitmapImage::Create(source_image).get());
  image_element->SetImageForTest(original_image_resource);

  base::Optional<IntRect> crop_rect =
      IntRect(0, 0, source_image->width(), source_image->height());

  for (int conversion_iterator = kColorSpaceConversion_Default;
       conversion_iterator <= kColorSpaceConversion_Last;
       conversion_iterator++) {
    ImageBitmapOptions options;
    options.setColorSpaceConversion(
        ColorCorrectionTestUtils::ColorSpaceConversionToString(
            static_cast<ColorSpaceConversion>(conversion_iterator)));
    ImageBitmap* image_bitmap = ImageBitmap::Create(
        image_element, crop_rect, &(image_element->GetDocument()), options);
    ASSERT_TRUE(image_bitmap);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkPixmap converted_pixmap;
    converted_image->peekPixels(&converted_pixmap);
    unsigned num_pixels = source_image->width() * source_image->height();

    if (conversion_iterator == kColorSpaceConversion_Preserve) {
      EXPECT_TRUE(
          SkColorSpace::Equals(colorspin.get(), converted_image->colorSpace()));
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), source_pixmap.addr(), num_pixels,
          kPixelFormat_8888, kAlphaMultiplied, kUnpremulRoundTripTolerance);
    } else {
      sk_sp<SkColorSpace> color_space =
          ColorCorrectionTestUtils::ColorSpaceConversionToSkColorSpace(
              static_cast<ColorSpaceConversion>(conversion_iterator));
      EXPECT_TRUE(SkColorSpace::Equals(color_space.get(),
                                       converted_image->colorSpace()));

      SkImageInfo expected_image_info =
          source_pixmap.info().makeColorSpace(color_space);
      if (color_space->gammaIsLinear()) {
        expected_image_info =
            expected_image_info.makeColorType(kRGBA_F16_SkColorType);
      }
      SkBitmap expected_bitmap;
      EXPECT_TRUE(expected_bitmap.tryAllocPixels(expected_image_info));
      source_image->readPixels(expected_bitmap.pixmap(), 0, 0);

      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), expected_bitmap.pixmap().addr(), num_pixels,
          color_space->gammaIsLinear() ? kPixelFormat_hhhh : kPixelFormat_8888,
          kAlphaMultiplied, kUnpremulRoundTripTolerance);
    }
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionImageBitmap) {
  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> colorspin =
      ColorCorrectionTestUtils::ColorSpinSkColorSpace();

  SkImageInfo image_info = SkImageInfo::MakeN32Premul(10, 10, colorspin);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(image_info));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> source_image = surface->makeImageSnapshot();
  SkPixmap source_pixmap;
  source_image->peekPixels(&source_pixmap);

  base::Optional<IntRect> crop_rect =
      IntRect(0, 0, source_image->width(), source_image->height());
  ImageBitmapOptions options;
  options.setColorSpaceConversion(
      ColorCorrectionTestUtils::ColorSpaceConversionToString(
          kColorSpaceConversion_Preserve));
  ImageBitmap* source_image_bitmap = ImageBitmap::Create(
      StaticBitmapImage::Create(source_image), crop_rect, options);
  ASSERT_TRUE(source_image_bitmap);

  for (int conversion_iterator = kColorSpaceConversion_Default;
       conversion_iterator <= kColorSpaceConversion_Last;
       conversion_iterator++) {
    options.setColorSpaceConversion(
        ColorCorrectionTestUtils::ColorSpaceConversionToString(
            static_cast<ColorSpaceConversion>(conversion_iterator)));
    ImageBitmap* image_bitmap =
        ImageBitmap::Create(source_image_bitmap, crop_rect, options);
    ASSERT_TRUE(image_bitmap);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkPixmap converted_pixmap;
    converted_image->peekPixels(&converted_pixmap);
    unsigned num_pixels = source_image->width() * source_image->height();

    if (conversion_iterator == kColorSpaceConversion_Preserve) {
      EXPECT_TRUE(
          SkColorSpace::Equals(colorspin.get(), converted_image->colorSpace()));
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), source_pixmap.addr(), num_pixels,
          kPixelFormat_8888, kAlphaMultiplied, kUnpremulRoundTripTolerance);
    } else {
      sk_sp<SkColorSpace> color_space =
          ColorCorrectionTestUtils::ColorSpaceConversionToSkColorSpace(
              static_cast<ColorSpaceConversion>(conversion_iterator));
      EXPECT_TRUE(SkColorSpace::Equals(color_space.get(),
                                       converted_image->colorSpace()));

      SkImageInfo expected_image_info =
          source_pixmap.info().makeColorSpace(color_space);
      if (color_space->gammaIsLinear()) {
        expected_image_info =
            expected_image_info.makeColorType(kRGBA_F16_SkColorType);
      }
      SkBitmap expected_bitmap;
      EXPECT_TRUE(expected_bitmap.tryAllocPixels(expected_image_info));
      source_image->readPixels(expected_bitmap.pixmap(), 0, 0);

      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), expected_bitmap.pixmap().addr(), num_pixels,
          color_space->gammaIsLinear() ? kPixelFormat_hhhh : kPixelFormat_8888,
          kAlphaMultiplied, kUnpremulRoundTripTolerance);
    }
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionStaticBitmapImage) {
  SkPaint p;
  p.setColor(SK_ColorRED);
  sk_sp<SkColorSpace> colorspin =
      ColorCorrectionTestUtils::ColorSpinSkColorSpace();

  SkImageInfo image_info = SkImageInfo::MakeN32Premul(10, 10, colorspin);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(image_info));
  surface->getCanvas()->drawCircle(5, 5, 5, p);
  sk_sp<SkImage> source_image = surface->makeImageSnapshot();
  SkPixmap source_pixmap;
  source_image->peekPixels(&source_pixmap);

  base::Optional<IntRect> crop_rect =
      IntRect(0, 0, source_image->width(), source_image->height());

  for (int conversion_iterator = kColorSpaceConversion_Default;
       conversion_iterator <= kColorSpaceConversion_Last;
       conversion_iterator++) {
    ImageBitmapOptions options;
    options.setColorSpaceConversion(
        ColorCorrectionTestUtils::ColorSpaceConversionToString(
            static_cast<ColorSpaceConversion>(conversion_iterator)));
    ImageBitmap* image_bitmap = ImageBitmap::Create(
        StaticBitmapImage::Create(source_image), crop_rect, options);
    ASSERT_TRUE(image_bitmap);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkPixmap converted_pixmap;
    converted_image->peekPixels(&converted_pixmap);
    unsigned num_pixels = source_image->width() * source_image->height();

    if (conversion_iterator == kColorSpaceConversion_Preserve) {
      EXPECT_TRUE(
          SkColorSpace::Equals(colorspin.get(), converted_image->colorSpace()));
      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), source_pixmap.addr(), num_pixels,
          kPixelFormat_8888, kAlphaMultiplied, kUnpremulRoundTripTolerance);
    } else {
      sk_sp<SkColorSpace> color_space =
          ColorCorrectionTestUtils::ColorSpaceConversionToSkColorSpace(
              static_cast<ColorSpaceConversion>(conversion_iterator));
      EXPECT_TRUE(SkColorSpace::Equals(color_space.get(),
                                       converted_image->colorSpace()));

      SkImageInfo expected_image_info =
          source_pixmap.info().makeColorSpace(color_space);
      if (color_space->gammaIsLinear()) {
        expected_image_info =
            expected_image_info.makeColorType(kRGBA_F16_SkColorType);
      }
      SkBitmap expected_bitmap;
      EXPECT_TRUE(expected_bitmap.tryAllocPixels(expected_image_info));
      source_image->readPixels(expected_bitmap.pixmap(), 0, 0);

      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), expected_bitmap.pixmap().addr(), num_pixels,
          color_space->gammaIsLinear() ? kPixelFormat_hhhh : kPixelFormat_8888,
          kAlphaMultiplied, kUnpremulRoundTripTolerance);
    }
  }
}

TEST_F(ImageBitmapTest, ImageBitmapColorSpaceConversionImageData) {
  unsigned char data_buffer[4] = {32, 96, 160, 128};
  DOMUint8ClampedArray* data = DOMUint8ClampedArray::Create(data_buffer, 4);
  ImageDataColorSettings color_settings;
  ImageData* image_data = ImageData::Create(
      IntSize(1, 1), NotShared<DOMUint8ClampedArray>(data), &color_settings);

  SkImageInfo image_info =
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType,
                        SkColorSpace::MakeSRGB());
  SkBitmap source_bitmap;
  EXPECT_TRUE(source_bitmap.tryAllocPixels(image_info));
  SkPixmap source_pixmap;
  source_pixmap = source_bitmap.pixmap();
  memcpy(source_pixmap.writable_addr(), image_data->BufferBase()->Data(), 4);

  base::Optional<IntRect> crop_rect = IntRect(0, 0, 1, 1);

  for (int conversion_iterator = kColorSpaceConversion_Default;
       conversion_iterator <= kColorSpaceConversion_Last;
       conversion_iterator++) {
    ImageBitmapOptions options;
    options.setColorSpaceConversion(
        ColorCorrectionTestUtils::ColorSpaceConversionToString(
            static_cast<ColorSpaceConversion>(conversion_iterator)));
    ImageBitmap* image_bitmap =
        ImageBitmap::Create(image_data, crop_rect, options);
    ASSERT_TRUE(image_bitmap);
    sk_sp<SkImage> converted_image =
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
    SkPixmap converted_pixmap;
    converted_image->peekPixels(&converted_pixmap);
    unsigned num_pixels = converted_image->width() * converted_image->height();

    if (conversion_iterator == kColorSpaceConversion_Preserve) {
      // crbug.com/886999
      // EXPECT_TRUE(SkColorSpace::Equals(SkColorSpace::MakeSRGB().get(),
      //                                  converted_image->colorSpace()));
      SkBitmap expected_bitmap;
      EXPECT_TRUE(expected_bitmap.tryAllocPixels(converted_pixmap.info()));
      source_pixmap.readPixels(expected_bitmap.pixmap(), 0, 0);

      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), expected_bitmap.pixmap().addr(), num_pixels,
          kPixelFormat_8888, kAlphaMultiplied, kUnpremulRoundTripTolerance);
    } else {
      sk_sp<SkColorSpace> color_space =
          ColorCorrectionTestUtils::ColorSpaceConversionToSkColorSpace(
              static_cast<ColorSpaceConversion>(conversion_iterator));
      // crbug.com/886999
      // EXPECT_TRUE(SkColorSpace::Equals(color_space.get(),
      //                                  converted_image->colorSpace()));
      SkBitmap expected_bitmap;
      EXPECT_TRUE(expected_bitmap.tryAllocPixels(converted_pixmap.info()));
      source_pixmap.readPixels(expected_bitmap.pixmap(), 0, 0);

      ColorCorrectionTestUtils::CompareColorCorrectedPixels(
          converted_pixmap.addr(), expected_bitmap.pixmap().addr(), num_pixels,
          color_space->gammaIsLinear() ? kPixelFormat_hhhh : kPixelFormat_8888,
          kAlphaMultiplied, kUnpremulRoundTripTolerance);
    }
  }
}

TEST_F(ImageBitmapTest, ImageBitmapPixelFormat) {
  SkImageInfo info = SkImageInfo::MakeS32(10, 10, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(info));
  sk_sp<SkImage> sk_image = surface->makeImageSnapshot();
  scoped_refptr<StaticBitmapImage> bitmap_image =
      StaticBitmapImage::Create(sk_image);

  // source: uint8, bitmap pixel format: default
  ImageBitmapOptions options;
  ImageBitmap* image_bitmap =
      ImageBitmap::Create(bitmap_image, bitmap_image->Rect(), options);

  ASSERT_TRUE(image_bitmap);
  sk_sp<SkImage> sk_image_internal =
      image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
  ASSERT_EQ(kN32_SkColorType, sk_image_internal->colorType());

  // source: uint8, bitmap pixel format: uint8
  options.setImagePixelFormat("uint8");
  ImageBitmap* image_bitmap_8888 =
      ImageBitmap::Create(bitmap_image, bitmap_image->Rect(), options);
  ASSERT_TRUE(image_bitmap_8888);
  sk_sp<SkImage> sk_image_internal_8888 = image_bitmap_8888->BitmapImage()
                                              ->PaintImageForCurrentFrame()
                                              .GetSkImage();
  ASSERT_EQ(kN32_SkColorType, sk_image_internal_8888->colorType());

  // Since there is no conversion from uint8 to default for image bitmap pixel
  // format option, we expect the two image bitmaps to refer to the same
  // internal SkImage back storage.
  ASSERT_EQ(sk_image_internal, sk_image_internal_8888);

  sk_sp<SkColorSpace> p3_color_space = SkColorSpace::MakeRGB(
      SkColorSpace::kLinear_RenderTargetGamma, SkColorSpace::kDCIP3_D65_Gamut);
  SkImageInfo info_f16 = SkImageInfo::Make(10, 10, kRGBA_F16_SkColorType,
                                           kPremul_SkAlphaType, p3_color_space);
  sk_sp<SkSurface> surface_f16(SkSurface::MakeRaster(info_f16));
  sk_sp<SkImage> sk_image_f16 = surface_f16->makeImageSnapshot();
  scoped_refptr<StaticBitmapImage> bitmap_image_f16 =
      StaticBitmapImage::Create(sk_image_f16);

  // source: f16, bitmap pixel format: default
  ImageBitmapOptions options_f16;
  ImageBitmap* image_bitmap_f16 = ImageBitmap::Create(
      bitmap_image_f16, bitmap_image_f16->Rect(), options_f16);
  ASSERT_TRUE(image_bitmap_f16);
  sk_sp<SkImage> sk_image_internal_f16 =
      image_bitmap_f16->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
  ASSERT_EQ(kRGBA_F16_SkColorType, sk_image_internal_f16->colorType());

  // source: f16, bitmap pixel format: uint8
  options_f16.setImagePixelFormat("uint8");
  ImageBitmap* image_bitmap_f16_8888 = ImageBitmap::Create(
      bitmap_image_f16, bitmap_image_f16->Rect(), options_f16);
  ASSERT_TRUE(image_bitmap_f16_8888);
  sk_sp<SkImage> sk_image_internal_f16_8888 =
      image_bitmap_f16_8888->BitmapImage()
          ->PaintImageForCurrentFrame()
          .GetSkImage();
  ASSERT_EQ(kN32_SkColorType, sk_image_internal_f16_8888->colorType());
}

// This test is failing on asan-clang-phone because memory allocation is
// declined. See <http://crbug.com/782286>.
#if defined(OS_ANDROID)
#define MAYBE_CreateImageBitmapFromTooBigImageDataDoesNotCrash \
  DISABLED_CreateImageBitmapFromTooBigImageDataDoesNotCrash
#else
#define MAYBE_CreateImageBitmapFromTooBigImageDataDoesNotCrash \
  CreateImageBitmapFromTooBigImageDataDoesNotCrash
#endif

// This test verifies if requesting a large ImageData and creating an
// ImageBitmap from that does not crash. crbug.com/780358
TEST_F(ImageBitmapTest,
       MAYBE_CreateImageBitmapFromTooBigImageDataDoesNotCrash) {
  ImageData* image_data =
      ImageData::CreateForTest(IntSize(v8::TypedArray::kMaxLength / 16, 1));
  DCHECK(image_data);
  ImageBitmapOptions options;
  options.setColorSpaceConversion(
      ColorCorrectionTestUtils::ColorSpaceConversionToString(
          kColorSpaceConversion_Default));
  ImageBitmap* image_bitmap = ImageBitmap::Create(
      image_data, IntRect(IntPoint(0, 0), image_data->Size()), options);
  DCHECK(image_bitmap);
}

}  // namespace blink
