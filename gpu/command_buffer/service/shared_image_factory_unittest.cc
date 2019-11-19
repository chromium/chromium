// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageFactoryTest : public testing::Test {
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
    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    factory_ = std::make_unique<SharedImageFactory>(
        preferences, workarounds, GpuFeatureInfo(), nullptr, &mailbox_manager_,
        &shared_image_manager_, &image_factory_, nullptr,
        /*enable_wrapped_sk_image=*/false);
  }

  void TearDown() override {
    factory_->DestroyAllSharedImages(true);
    factory_.reset();
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  gles2::MailboxManagerImpl mailbox_manager_;
  TextureImageFactory image_factory_;
  std::unique_ptr<SharedImageFactory> factory_;
  SharedImageManager shared_image_manager_;
};

TEST_F(SharedImageFactoryTest, Basic) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  // Validation of the produced backing/mailbox is handled in individual backing
  // factory unittests.
  ASSERT_TRUE(texture_base);
  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

TEST_F(SharedImageFactoryTest, DuplicateMailbox) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  EXPECT_TRUE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));
  EXPECT_FALSE(
      factory_->CreateSharedImage(mailbox, format, size, color_space, usage));

  GpuPreferences preferences;
  GpuDriverBugWorkarounds workarounds;
  workarounds.max_texture_size = INT_MAX - 1;
  auto other_factory = std::make_unique<SharedImageFactory>(
      preferences, workarounds, GpuFeatureInfo(), nullptr, &mailbox_manager_,
      &shared_image_manager_, &image_factory_, nullptr,
      /*enable_wrapped_sk_image=*/false);
  EXPECT_FALSE(other_factory->CreateSharedImage(mailbox, format, size,
                                                color_space, usage));
}

TEST_F(SharedImageFactoryTest, DestroyInexistentMailbox) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  EXPECT_FALSE(factory_->DestroySharedImage(mailbox));
}

}  // anonymous namespace
}  // namespace gpu
