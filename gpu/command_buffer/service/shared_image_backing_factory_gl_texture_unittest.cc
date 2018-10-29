// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageBackingFactoryGLTextureTest
    : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = use_passthrough();
    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    backing_factory_ = std::make_unique<SharedImageBackingFactoryGLTexture>(
        preferences, workarounds, GpuFeatureInfo(), &image_factory_, nullptr);

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = new raster::RasterDecoderContextState(
        std::move(share_group), surface_, context_,
        false /* use_virtualized_gl_contexts */);
    context_state_->InitializeGrContext(workarounds, nullptr);
  }

  bool use_passthrough() {
    return GetParam() && gles2::PassthroughCommandDecoderSupported();
  }

  GrContext* gr_context() { return context_state_->gr_context.get(); }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<raster::RasterDecoderContextState> context_state_;
  TextureImageFactory image_factory_;
  std::unique_ptr<SharedImageBackingFactoryGLTexture> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  SharedImageManager shared_image_manager_;
};

TEST_P(SharedImageBackingFactoryGLTextureTest, Basic) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a legacy mailbox.
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum expected_target = GL_TEXTURE_2D;
  EXPECT_EQ(texture_base->target(), expected_target);
  if (!use_passthrough()) {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    EXPECT_TRUE(texture->IsImmutable());
    int width, height, depth;
    bool has_level =
        texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
    EXPECT_TRUE(has_level);
    EXPECT_EQ(width, size.width());
    EXPECT_EQ(height, size.height());
  }

  // Next, validate via a SharedImageRepresentationGLTexture.
  EXPECT_TRUE(shared_image_manager_.Register(std::move(backing)));
  if (!use_passthrough()) {
    auto gl_representation = shared_image_manager_.ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexture()->service_id());
    EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Next, validate a SharedImageRepresentationGLTexturePassthrough.
  if (use_passthrough()) {
    auto gl_representation =
        shared_image_manager_.ProduceGLTexturePassthrough(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Finally, validate a SharedImageRepresentationSkia.
  auto skia_representation = shared_image_manager_.ProduceSkia(mailbox);
  EXPECT_TRUE(skia_representation);
  auto surface = skia_representation->BeginWriteAccess(
      gr_context(), 0, kRGBA_8888_SkColorType,
      SkSurfaceProps(0, kUnknown_SkPixelGeometry));
  EXPECT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  skia_representation->EndWriteAccess(std::move(surface));
  GrBackendTexture backend_texture;
  EXPECT_TRUE(skia_representation->BeginReadAccess(

      kRGBA_8888_SkColorType, &backend_texture));
  EXPECT_EQ(size.width(), backend_texture.width());
  EXPECT_EQ(size.width(), backend_texture.width());
  skia_representation->EndReadAccess();
  skia_representation.reset();

  shared_image_manager_.Unregister(mailbox);
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

TEST_P(SharedImageBackingFactoryGLTextureTest, Image) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a legacy mailbox.
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum target = texture_base->target();
  scoped_refptr<gl::GLImage> image;
  if (use_passthrough()) {
    auto* texture = static_cast<gles2::TexturePassthrough*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  } else {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  }
  ASSERT_TRUE(image);
  EXPECT_EQ(size, image->GetSize());

  // Next, validate via a SharedImageRepresentationGLTexture.
  EXPECT_TRUE(shared_image_manager_.Register(std::move(backing)));
  if (!use_passthrough()) {
    auto gl_representation = shared_image_manager_.ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexture()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Next, validate a SharedImageRepresentationGLTexturePassthrough.
  if (use_passthrough()) {
    auto gl_representation =
        shared_image_manager_.ProduceGLTexturePassthrough(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Finally, validate a SharedImageRepresentationSkia.
  auto skia_representation = shared_image_manager_.ProduceSkia(mailbox);
  EXPECT_TRUE(skia_representation);
  auto surface = skia_representation->BeginWriteAccess(
      gr_context(), 0, kRGBA_8888_SkColorType,
      SkSurfaceProps(0, kUnknown_SkPixelGeometry));
  EXPECT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  skia_representation->EndWriteAccess(std::move(surface));
  GrBackendTexture backend_texture;
  EXPECT_TRUE(skia_representation->BeginReadAccess(

      kRGBA_8888_SkColorType, &backend_texture));
  EXPECT_EQ(size.width(), backend_texture.width());
  EXPECT_EQ(size.width(), backend_texture.width());
  skia_representation->EndReadAccess();
  skia_representation.reset();

  shared_image_manager_.Unregister(mailbox);
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));

  if (!use_passthrough()) {
    // Create a R-8 image texture, and check that the internal_format is that of
    // the image (GL_RGBA for TextureImageFactory). This only matters for the
    // validating decoder.
    auto format = viz::ResourceFormat::RED_8;
    backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                  color_space, usage);
    EXPECT_TRUE(backing);
    EXPECT_TRUE(shared_image_manager_.Register(std::move(backing)));
    auto gl_representation = shared_image_manager_.ProduceGLTexture(mailbox);
    ASSERT_TRUE(gl_representation);
    gles2::Texture* texture = gl_representation->GetTexture();
    ASSERT_TRUE(texture);
    GLenum type = 0;
    GLenum internal_format = 0;
    EXPECT_TRUE(texture->GetLevelType(target, 0, &type, &internal_format));
    EXPECT_EQ(internal_format, static_cast<GLenum>(GL_RGBA));
    gl_representation.reset();
    shared_image_manager_.Unregister(mailbox);
  }
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InvalidFormat) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::UYVY_422;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_FALSE(backing);
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InvalidSize) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                color_space, usage);
  EXPECT_FALSE(backing);
}

INSTANTIATE_TEST_CASE_P(Service,
                        SharedImageBackingFactoryGLTextureTest,
                        ::testing::Bool());

}  // anonymous namespace
}  // namespace gpu
