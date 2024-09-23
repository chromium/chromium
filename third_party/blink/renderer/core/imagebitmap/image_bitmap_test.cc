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
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ExceptionState;

class ImageBitmapTest : public testing::Test {
 protected:
  void SetUp() override {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(10, 10));
    surface->getCanvas()->clear(0xFFFFFFFF);
    image_ = surface->makeImageSnapshot();

    sk_sp<SkSurface> surface2 =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(5, 5));
    surface2->getCanvas()->clear(0xAAAAAAAA);
    image2_ = surface2->makeImageSnapshot();

    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ =
        ReplaceMemoryCacheForTesting(MakeGarbageCollected<MemoryCache>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()));

    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContextGLES2(test_context_provider_.get());
  }

  void TearDown() override {
    // Garbage collection is required prior to switching out the
    // test's memory cache; image resources are released, evicting
    // them from the cache.
    ThreadState::Current()->CollectAllGarbageForTesting(
        ThreadState::StackState::kNoHeapPointers);

    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
    SharedGpuContext::Reset();
  }

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  sk_sp<SkImage> image_, image2_;
  Persistent<MemoryCache> global_memory_cache_;
};

TEST_F(ImageBitmapTest, ImageResourceConsistency) {
  const ImageBitmapOptions* default_options = ImageBitmapOptions::Create();
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  auto* image_element =
      MakeGarbageCollected<HTMLImageElement>(dummy->GetDocument());
  sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(5, 5, src_rgb_color_space);
  sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  ImageResourceContent* original_image_content =
      ImageResourceContent::CreateLoaded(
          UnacceleratedStaticBitmapImage::Create(image).get());
  image_element->SetImageForTest(original_image_content);

  std::optional<gfx::Rect> crop_rect =
      gfx::Rect(0, 0, image_element->width(), image_element->height());
  auto* image_bitmap_no_crop = MakeGarbageCollected<ImageBitmap>(
      image_element, crop_rect, default_options);
  ASSERT_TRUE(image_bitmap_no_crop);
  crop_rect =
      gfx::Rect(image_element->width() / 2, image_element->height() / 2,
                image_element->width() / 2, image_element->height() / 2);
  auto* image_bitmap_interior_crop = MakeGarbageCollected<ImageBitmap>(
      image_element, crop_rect, default_options);
  ASSERT_TRUE(image_bitmap_interior_crop);
  crop_rect =
      gfx::Rect(-image_element->width() / 2, -image_element->height() / 2,
                image_element->width(), image_element->height());
  auto* image_bitmap_exterior_crop = MakeGarbageCollected<ImageBitmap>(
      image_element, crop_rect, default_options);
  ASSERT_TRUE(image_bitmap_exterior_crop);
  crop_rect = gfx::Rect(-image_element->width(), -image_element->height(),
                        image_element->width(), image_element->height());
  auto* image_bitmap_outside_crop = MakeGarbageCollected<ImageBitmap>(
      image_element, crop_rect, default_options);
  ASSERT_TRUE(image_bitmap_outside_crop);

  ASSERT_EQ(image_bitmap_no_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage());
  ASSERT_NE(image_bitmap_interior_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage());
  ASSERT_NE(image_bitmap_exterior_crop->BitmapImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage());

  scoped_refptr<StaticBitmapImage> empty_image =
      image_bitmap_outside_crop->BitmapImage();
  ASSERT_NE(empty_image->PaintImageForCurrentFrame().GetSwSkImage(),
            image_element->CachedImage()
                ->GetImage()
                ->PaintImageForCurrentFrame()
                .GetSwSkImage());
}

// Verifies that ImageBitmaps constructed from HTMLImageElements hold a
// reference to the original Image if the HTMLImageElement src is changed.
TEST_F(ImageBitmapTest, ImageBitmapSourceChanged) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  auto* image = MakeGarbageCollected<HTMLImageElement>(dummy->GetDocument());
  sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(5, 5, src_rgb_color_space);
  sk_sp<SkSurface> raster_surface(SkSurfaces::Raster(raster_image_info));
  sk_sp<SkImage> raster_image = raster_surface->makeImageSnapshot();
  ImageResourceContent* original_image_content =
      ImageResourceContent::CreateLoaded(
          UnacceleratedStaticBitmapImage::Create(raster_image).get());
  image->SetImageForTest(original_image_content);

  const ImageBitmapOptions* default_options = ImageBitmapOptions::Create();
  std::optional<gfx::Rect> crop_rect =
      gfx::Rect(0, 0, image->width(), image->height());
  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image, crop_rect, default_options);
  ASSERT_TRUE(image_bitmap);
  ASSERT_EQ(
      image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage(),
      original_image_content->GetImage()
          ->PaintImageForCurrentFrame()
          .GetSwSkImage());

  ImageResourceContent* new_image_content = ImageResourceContent::CreateLoaded(
      UnacceleratedStaticBitmapImage::Create(image2_).get());
  image->SetImageForTest(new_image_content);

  {
    ASSERT_EQ(
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage(),
        original_image_content->GetImage()
            ->PaintImageForCurrentFrame()
            .GetSwSkImage());
    SkImage* image1 = image_bitmap->BitmapImage()
                          ->PaintImageForCurrentFrame()
                          .GetSwSkImage()
                          .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 = original_image_content->GetImage()
                          ->PaintImageForCurrentFrame()
                          .GetSwSkImage()
                          .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_EQ(image1, image2);
  }

  {
    ASSERT_NE(
        image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage(),
        new_image_content->GetImage()
            ->PaintImageForCurrentFrame()
            .GetSwSkImage());
    SkImage* image1 = image_bitmap->BitmapImage()
                          ->PaintImageForCurrentFrame()
                          .GetSwSkImage()
                          .get();
    ASSERT_NE(image1, nullptr);
    SkImage* image2 = new_image_content->GetImage()
                          ->PaintImageForCurrentFrame()
                          .GetSwSkImage()
                          .get();
    ASSERT_NE(image2, nullptr);
    ASSERT_NE(image1, image2);
  }
}

static void TestImageBitmapTextureBacked(
    scoped_refptr<StaticBitmapImage> bitmap,
    gfx::Rect& rect,
    ImageBitmapOptions* options,
    bool is_texture_backed) {
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(bitmap, rect, options);
  EXPECT_TRUE(image_bitmap);
  EXPECT_EQ(image_bitmap->BitmapImage()->IsTextureBacked(), is_texture_backed);
}

TEST_F(ImageBitmapTest, AvoidGPUReadback) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
      SkImageInfo::MakeN32Premul(100, 100), cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kNo, context_provider_wrapper,
      RasterMode::kGPU, gpu::SharedImageUsageSet());

  scoped_refptr<StaticBitmapImage> bitmap =
      resource_provider->Snapshot(FlushReason::kTesting);
  ASSERT_TRUE(bitmap->IsTextureBacked());

  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(bitmap);
  EXPECT_TRUE(image_bitmap);
  EXPECT_TRUE(image_bitmap->BitmapImage()->IsTextureBacked());

  gfx::Rect image_bitmap_rect(25, 25, 50, 50);
  {
    ImageBitmapOptions* image_bitmap_options = ImageBitmapOptions::Create();
    TestImageBitmapTextureBacked(bitmap, image_bitmap_rect,
                                 image_bitmap_options, true);
  }

  std::list<String> image_orientations = {"none", "flipY"};
  std::list<String> premultiply_alphas = {"none", "premultiply", "default"};
  std::list<String> color_space_conversions = {"none", "default"};
  std::list<int> resize_widths = {25, 50, 75};
  std::list<int> resize_heights = {25, 50, 75};
  std::list<String> resize_qualities = {"pixelated", "low", "medium", "high"};

  for (auto image_orientation : image_orientations) {
    for (auto premultiply_alpha : premultiply_alphas) {
      for (auto color_space_conversion : color_space_conversions) {
        for (auto resize_width : resize_widths) {
          for (auto resize_height : resize_heights) {
            for (auto resize_quality : resize_qualities) {
              ImageBitmapOptions* image_bitmap_options =
                  ImageBitmapOptions::Create();
              image_bitmap_options->setImageOrientation(image_orientation);
              image_bitmap_options->setPremultiplyAlpha(premultiply_alpha);
              image_bitmap_options->setColorSpaceConversion(
                  color_space_conversion);
              image_bitmap_options->setResizeWidth(resize_width);
              image_bitmap_options->setResizeHeight(resize_height);
              image_bitmap_options->setResizeQuality(resize_quality);
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

// This test is failing on asan-clang-phone because memory allocation is
// declined. See <http://crbug.com/782286>.
// This test is failing on fuchsia because memory allocation is
// declined.  <http://crbug.com/1090252>.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
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
  constexpr int kWidth = 1 << 28;  // 256M pixels width, resulting in 1GB data.
  ImageData* image_data = ImageData::CreateForTest(gfx::Size(kWidth, 1));
  DCHECK(image_data);
  ImageBitmapOptions* options = ImageBitmapOptions::Create();
  options->setColorSpaceConversion("default");
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      image_data, gfx::Rect(image_data->Size()), options);
  DCHECK(image_bitmap);
}

TEST_F(ImageBitmapTest, ImageAlphaState) {
  auto dummy = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  auto* image_element =
      MakeGarbageCollected<HTMLImageElement>(dummy->GetDocument());

  // Load a 2x2 png file which has pixels (255, 102, 153, 0). It is a fully
  // transparent image.
  ResourceRequest resource_request(
      "data:image/"
      "png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEUlEQVR42mP8nzaTAQQYYQwA"
      "LssD/5ca+r8AAAAASUVORK5CYII=");

  FetchParameters params =
      FetchParameters::CreateForTest(std::move(resource_request));

  ImageResourceContent* resource_content =
      ImageResourceContent::Fetch(params, dummy->GetDocument().Fetcher());

  image_element->SetImageForTest(resource_content);

  ImageBitmapOptions* options = ImageBitmapOptions::Create();
  // ImageBitmap created from unpremul source image result.
  options->setPremultiplyAlpha("none");

  // Additional operation shouldn't affect alpha op.
  options->setImageOrientation("flipY");

  std::optional<gfx::Rect> crop_rect =
      gfx::Rect(0, 0, image_element->width(), image_element->height());
  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image_element, crop_rect, options);
  ASSERT_TRUE(image_bitmap);

  // Read 1 pixel
  sk_sp<SkImage> result =
      image_bitmap->BitmapImage()->PaintImageForCurrentFrame().GetSwSkImage();
  SkPixmap pixmap;
  ASSERT_TRUE(result->peekPixels(&pixmap));
  const uint32_t* pixels = pixmap.addr32();

  SkColorType result_color_type = result->colorType();
  SkColor expected = SkColorSetARGB(0, 0, 0, 0);

  switch (result_color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      // Set ABGR value as reverse of RGBA
      expected =
          SkColorSetARGB(/* a */ 0, /* b */ 153, /* g */ 102, /* r */ 255);
      break;
    case SkColorType::kBGRA_8888_SkColorType:
      // Set ARGB value as reverse of BGRA
      expected =
          SkColorSetARGB(/* a */ 0, /* r */ 255, /* g */ 102, /* b */ 153);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  ASSERT_EQ(pixels[0], expected);
}

}  // namespace blink
