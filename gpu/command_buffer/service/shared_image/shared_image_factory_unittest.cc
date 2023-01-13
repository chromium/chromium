// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                  gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuPreferences preferences;
    GpuDriverBugWorkarounds workarounds;
    factory_ = std::make_unique<SharedImageFactory>(
        preferences, workarounds, GpuFeatureInfo(), nullptr,
        &shared_image_manager_, nullptr,
        /*is_for_display_compositor=*/false);
  }

  void TearDown() override {
    factory_->DestroyAllSharedImages(true);
    factory_.reset();
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<SharedImageFactory> factory_;
  SharedImageManager shared_image_manager_;
};

TEST_F(SharedImageFactoryTest, Basic) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage));
  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
}

TEST_F(SharedImageFactoryTest, DuplicateMailbox) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage));
  EXPECT_FALSE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage));

  GpuPreferences preferences;
  GpuDriverBugWorkarounds workarounds;
  auto other_factory = std::make_unique<SharedImageFactory>(
      preferences, workarounds, GpuFeatureInfo(), nullptr,
      &shared_image_manager_, nullptr,
      /*is_for_display_compositor=*/false);
  EXPECT_FALSE(other_factory->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage));
}

TEST_F(SharedImageFactoryTest, DestroyInexistentMailbox) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  EXPECT_FALSE(factory_->DestroySharedImage(mailbox));
}

}  // anonymous namespace
}  // namespace gpu
