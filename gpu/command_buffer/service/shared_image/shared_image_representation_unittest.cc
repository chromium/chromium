// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"

namespace gpu {

class SharedImageRepresentationTest : public ::testing::Test {
 public:
  void SetUp() override {
    tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    mailbox_ = Mailbox::Generate();
    auto format = viz::SinglePlaneFormat::kRGBA_8888;
    gfx::Size size(256, 256);
    auto color_space = gfx::ColorSpace::CreateSRGB();
    auto surface_origin = kTopLeft_GrSurfaceOrigin;
    auto alpha_type = kPremul_SkAlphaType;
    // Add the usages that the tests in this file require.
    SharedImageUsageSet usage = {
        SHARED_IMAGE_USAGE_GLES2_READ,   SHARED_IMAGE_USAGE_GLES2_WRITE,
        SHARED_IMAGE_USAGE_RASTER_READ,  SHARED_IMAGE_USAGE_RASTER_WRITE,
        SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_SCANOUT};
    auto backing = std::make_unique<TestImageBacking>(
        mailbox_, format, size, color_space, surface_origin, alpha_type, usage,
        /*estimated_size=*/0);
    factory_ref_ = manager_.Register(std::move(backing), tracker_.get());
  }

 protected:
  gpu::Mailbox mailbox_;
  SharedImageManager manager_;
  std::unique_ptr<MemoryTypeTracker> tracker_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref_;
};

TEST_F(SharedImageRepresentationTest, GLTextureClearing) {
  auto representation = manager_.ProduceGLTexture(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access when |allow_uncleared| is false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Begin/End access should not modify clear status on its own.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clearing underlying GL texture should clear the SharedImage.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    representation->GetTexture()->SetLevelCleared(GL_TEXTURE_2D, 0,
                                                  /*cleared=*/true);
  }
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }

  // Reset the representation to uncleared. This should unclear the texture on
  // BeginAccess.
  representation->SetClearedRect(gfx::Rect());
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    EXPECT_FALSE(
        representation->GetTexture()->IsLevelCleared(GL_TEXTURE_2D, 0));
  }
  EXPECT_FALSE(representation->IsCleared());
}

TEST_F(SharedImageRepresentationTest, GLTexturePassthroughClearing) {
  auto representation =
      manager_.ProduceGLTexturePassthrough(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access when |allow_uncleared| is false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Begin/End access will not clear the representation on its own.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin accdess with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
}

TEST_F(SharedImageRepresentationTest, SkiaClearing) {
  auto representation = manager_.ProduceSkia(mailbox_, tracker_.get(), nullptr);
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin read access.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin write access when |allow_uncleared| is
  // false.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr, SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We can begin write access when |allow_uncleared| is true.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin read access.
  {
    auto scoped_access =
        representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());

  // We can also begin write access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedWriteAccess(
        nullptr, nullptr, SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
}

TEST_F(SharedImageRepresentationTest, DawnClearing) {
  // TestImageBacking will return a dummy texture
  // wgpu::Texture(reinterpret_cast<WGPUTexture>(203)), so we have to override
  // the texture reference/release procs to avoid crashing.
  DawnProcTable procs = {};
  procs.textureAddRef = [](WGPUTexture) {};
  procs.textureRelease = [](WGPUTexture) {};
  dawnProcSetProcs(&procs);

  auto representation = manager_.ProduceDawn(
      mailbox_, tracker_.get(), /*device=*/nullptr, wgpu::BackendType::Null, {},
      /*context_state=*/nullptr);
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        wgpu::TextureUsage::None,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // We can begin access when |allow_uncleared| is true.
  {
    auto scoped_access = representation->BeginScopedAccess(
        wgpu::TextureUsage::None,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can also begin access with |allow_uncleared| == false.
  {
    auto scoped_access = representation->BeginScopedAccess(
        wgpu::TextureUsage::None,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
  dawnProcSetProcs(nullptr);
}

TEST_F(SharedImageRepresentationTest, OverlayClearing) {
  auto representation = manager_.ProduceOverlay(mailbox_, tracker_.get());
  EXPECT_FALSE(representation->IsCleared());

  // We should not be able to begin read ccess.
  {
    auto scoped_access = representation->BeginScopedReadAccess();
    EXPECT_FALSE(scoped_access);
  }
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage.
  representation->SetCleared();
  EXPECT_TRUE(representation->IsCleared());

  // We can now begin read access.
  {
    auto scoped_access = representation->BeginScopedReadAccess();
    EXPECT_TRUE(scoped_access);
  }
  EXPECT_TRUE(representation->IsCleared());
}

}  // namespace gpu
