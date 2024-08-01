// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
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

    context_state_ = base::MakeRefCounted<SharedContextState>(
        base::MakeRefCounted<gl::GLShareGroup>(), surface_, context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        GrContextType::kGL);

    GpuPreferences preferences;
    GpuDriverBugWorkarounds workarounds;

    bool initialize_gl = context_state_->InitializeGL(
        preferences, base::MakeRefCounted<gles2::FeatureInfo>(
                         workarounds, GpuFeatureInfo()));
    ASSERT_TRUE(initialize_gl);

    bool initialize_skia =
        context_state_->InitializeSkia(preferences, workarounds);
    ASSERT_TRUE(initialize_skia);

    factory_ = std::make_unique<SharedImageFactory>(
        preferences, workarounds, GpuFeatureInfo(), context_state_.get(),
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
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  SharedImageManager shared_image_manager_;
};

TEST_F(SharedImageFactoryTest, Basic) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage, "TestLabel"));
  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
}

TEST_F(SharedImageFactoryTest, DuplicateMailbox) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage, "TestLabel"));
  EXPECT_FALSE(factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage, "TestLabel"));

  GpuPreferences preferences;
  GpuDriverBugWorkarounds workarounds;
  auto other_factory = std::make_unique<SharedImageFactory>(
      preferences, workarounds, GpuFeatureInfo(), context_state_.get(),
      &shared_image_manager_, nullptr,
      /*is_for_display_compositor=*/false);
  EXPECT_FALSE(other_factory->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, surface_handle, usage, "TestLabel"));
}

TEST_F(SharedImageFactoryTest, DestroyInexistentMailbox) {
  auto mailbox = Mailbox::Generate();
  EXPECT_FALSE(factory_->DestroySharedImage(mailbox));
}

}  // anonymous namespace
}  // namespace gpu
