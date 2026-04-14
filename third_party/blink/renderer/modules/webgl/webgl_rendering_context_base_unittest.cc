// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/renderers/video_frame_shared_image_cache.h"
#include "media/renderers/video_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace blink {

namespace {

static base::HeapArray<uint8_t> ReadbackTexture(gpu::gles2::GLES2Interface* gl,
                                                GLuint texture,
                                                const gfx::Size& size) {
  size_t pixel_count = size.width() * size.height();
  GLuint fbo = 0;
  gl->GenFramebuffers(1, &fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
  auto pixels = base::HeapArray<uint8_t>::Uninit(pixel_count * 4);
  uint8_t* raw_pixels = pixels.data();
  gl->ReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                 raw_pixels);
  gl->DeleteFramebuffers(1, &fbo);
  return pixels;
}

static auto ColorGetter(base::span<uint8_t> pixels, const gfx::Size& size) {
  return [pixels, size](size_t x, size_t y) {
    base::span<uint8_t> p = pixels.subspan((size.width() * y + x) * 4);
    return SkColorSetARGB(p[3], p[0], p[1], p[2]);
  };
}

class WebGLRenderingContextBaseUnittest : public testing::Test {
 public:
  void SetUp() override {
    if (base::FeatureList::IsEnabled(::features::kVulkanFromANGLE)) {
      GTEST_SKIP() << "Temporarily skipped for Android Desktop devices. See: "
                      "crbug.com/440128352";
    }

    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
    enable_pixels_.emplace();
    media_context_ = base::MakeRefCounted<viz::TestInProcessContextProvider>(
        viz::TestContextType::kRaster, /*support_locking=*/false);
    gpu::ContextResult result = media_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    raster_context_ = base::MakeRefCounted<viz::TestInProcessContextProvider>(
        viz::TestContextType::kRaster, /*support_locking=*/false);
    result = raster_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    destination_context_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            viz::TestContextType::kGLES2, /*support_locking=*/false);
    result = destination_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    cropped_frame_ = media::CreateCroppedFrame();
  }

  void TearDown() override {
    rgb_shared_image_cache_.reset();
    yuv_shared_image_cache_.reset();
    destination_context_.reset();
    raster_context_.reset();
    media_context_.reset();
    enable_pixels_.reset();
    viz::TestGpuServiceHolder::ResetInstance();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }

  media::VideoFrameSharedImageCache* GetRGBSharedImageCache() {
    if (!rgb_shared_image_cache_) {
      rgb_shared_image_cache_ =
          std::make_unique<media::VideoFrameSharedImageCache>();
    }
    return rgb_shared_image_cache_.get();
  }

  media::VideoFrameSharedImageCache* GetYUVSharedImageCache() {
    if (!yuv_shared_image_cache_) {
      yuv_shared_image_cache_ =
          std::make_unique<media::VideoFrameSharedImageCache>();
    }
    return yuv_shared_image_cache_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::optional<gl::DisableNullDrawGLBindings> enable_pixels_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
  scoped_refptr<viz::TestInProcessContextProvider> media_context_;
  scoped_refptr<viz::TestInProcessContextProvider> raster_context_;
  scoped_refptr<viz::TestInProcessContextProvider> destination_context_;
  std::unique_ptr<media::VideoFrameSharedImageCache> rgb_shared_image_cache_;
  std::unique_ptr<media::VideoFrameSharedImageCache> yuv_shared_image_cache_;
  scoped_refptr<media::VideoFrame> cropped_frame_;
};

TEST_F(WebGLRenderingContextBaseUnittest, CopyVideoFrameYUVDataToGLTexture) {
  auto* destination_gl = destination_context_->ContextGL();
  DCHECK(destination_gl);
  GLenum target = GL_TEXTURE_2D;
  GLuint texture = 0;
  destination_gl->GenTextures(1, &texture);
  destination_gl->BindTexture(target, texture);

  media::PaintCanvasVideoRenderer::CopyVideoFrameYUVDataToGLTexture(
      media_context_.get(), destination_gl, cropped_frame_,
      GetRGBSharedImageCache(), GetYUVSharedImageCache(), target, texture,
      GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, 0, kUnpremul_SkAlphaType,
      kTopLeft_GrSurfaceOrigin);

  gfx::Size expected_size = cropped_frame_->visible_rect().size();

  base::HeapArray<uint8_t> pixels =
      ReadbackTexture(destination_gl, texture, expected_size);
  auto get_color = ColorGetter(pixels.as_span(), expected_size);

  // Avoid checking around the seams.
  EXPECT_EQ(SK_ColorBLACK, get_color(0, 0));
  EXPECT_EQ(SK_ColorRED, get_color(3, 0));
  EXPECT_EQ(SK_ColorRED, get_color(7, 0));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 3));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 5));
  EXPECT_EQ(SK_ColorBLUE, get_color(3, 3));
  EXPECT_EQ(SK_ColorBLUE, get_color(7, 5));

  destination_gl->DeleteTextures(1, &texture);
}

TEST_F(WebGLRenderingContextBaseUnittest,
       CopyVideoFrameYUVDataToGLTexture_FlipY) {
  auto* destination_gl = destination_context_->ContextGL();
  DCHECK(destination_gl);
  GLenum target = GL_TEXTURE_2D;
  GLuint texture = 0;
  destination_gl->GenTextures(1, &texture);
  destination_gl->BindTexture(target, texture);

  media::PaintCanvasVideoRenderer::CopyVideoFrameYUVDataToGLTexture(
      media_context_.get(), destination_gl, cropped_frame_,
      GetRGBSharedImageCache(), GetYUVSharedImageCache(), target, texture,
      GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, 0, kUnpremul_SkAlphaType,
      kBottomLeft_GrSurfaceOrigin);

  gfx::Size expected_size = cropped_frame_->visible_rect().size();

  base::HeapArray<uint8_t> pixels =
      ReadbackTexture(destination_gl, texture, expected_size);
  auto get_color = ColorGetter(pixels.as_span(), expected_size);

  // Avoid checking around the seams.
  EXPECT_EQ(SK_ColorBLACK, get_color(0, 5));
  EXPECT_EQ(SK_ColorRED, get_color(3, 5));
  EXPECT_EQ(SK_ColorRED, get_color(7, 5));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 2));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 0));
  EXPECT_EQ(SK_ColorBLUE, get_color(3, 2));
  EXPECT_EQ(SK_ColorBLUE, get_color(7, 0));

  destination_gl->DeleteTextures(1, &texture);
}

}  // namespace

}  // namespace blink
