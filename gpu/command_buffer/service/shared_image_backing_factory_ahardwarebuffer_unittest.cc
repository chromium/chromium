// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageBackingFactoryAHardwareBufferTest : public testing::Test {
 public:
  void SetUp() override {
    // AHardwareBuffer is only supported on ANDROID O+. Hence these tests
    // should not be run on android versions less that O.
    if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
      return;

    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    backing_factory_ =
        std::make_unique<SharedImageBackingFactoryAHardwareBuffer>(
            workarounds, GpuFeatureInfo(), nullptr);
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<SharedImageBackingFactoryAHardwareBuffer> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  SharedImageManager shared_image_manager_;
};

// Basic test to check creation and deletion of AHB backed shared image.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, Basic) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

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
  GLenum expected_target = GL_TEXTURE_EXTERNAL_OES;
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);

  // Currently there is no support for passthrough texture on android and hence
  // in AHB backing. So the TextureBase* should be pointing to a Texture object.
  auto* texture = gles2::Texture::CheckedCast(texture_base);
  ASSERT_TRUE(texture);
  EXPECT_EQ(texture->target(), expected_target);
  EXPECT_TRUE(texture->IsImmutable());
  int width, height, depth;
  bool has_level =
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
  EXPECT_TRUE(has_level);
  EXPECT_EQ(width, size.width());
  EXPECT_EQ(height, size.height());

  // Next validate via a SharedImageRepresentationGLTexture.
  EXPECT_TRUE(shared_image_manager_.Register(std::move(backing)));
  auto gl_representation = shared_image_manager_.ProduceGLTexture(mailbox);
  EXPECT_TRUE(gl_representation);
  EXPECT_TRUE(gl_representation->GetTexture()->service_id());
  EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
  EXPECT_EQ(size, gl_representation->size());
  EXPECT_EQ(format, gl_representation->format());
  EXPECT_EQ(color_space, gl_representation->color_space());
  EXPECT_EQ(usage, gl_representation->usage());
  gl_representation.reset();

  shared_image_manager_.Unregister(mailbox);
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

// Test to check invalid format support.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, InvalidFormat) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::UYVY_422;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_FALSE(backing);
}

// Test to check invalid size support.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, InvalidSize) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

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

}  // anonymous namespace
}  // namespace gpu
