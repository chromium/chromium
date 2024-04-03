// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

namespace {
// NOTE: If this test implementation starts to grow heavy, consider pulling the
// viz::TestSharedImageInterface implementation down into //gpu, unifying that
// with this one, and exposing that as a general-purpose test utility.
class TestSharedImageInterface : public SharedImageInterface {
 public:
  TestSharedImageInterface() = default;

  // SharedImageInterface:
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle) override {
    mailbox_for_most_recently_created_shared_image_ =
        gpu::Mailbox::GenerateForSharedImage();
    return base::MakeRefCounted<gpu::ClientSharedImage>(
        mailbox_for_most_recently_created_shared_image_, si_info.meta,
        SyncToken(), holder_, gfx::EMPTY_BUFFER);
  }
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) override {
    return nullptr;
  }
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage) override {
    return nullptr;
  }
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override {
    return nullptr;
  }
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) override {
    return nullptr;
  }
  SharedImageInterface::SharedImageMapping CreateSharedImage(
      const SharedImageInfo& si_info) override {
    return {nullptr, base::WritableSharedMemoryMapping()};
  }
  scoped_refptr<ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferPlane plane,
      const SharedImageInfo& si_info) override {
    return nullptr;
  }
  void UpdateSharedImage(const SyncToken& sync_token,
                         const Mailbox& mailbox) override {}
  void UpdateSharedImage(const SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const Mailbox& mailbox) override {}
  scoped_refptr<ClientSharedImage> ImportSharedImage(
      const ExportedSharedImage& exported_shared_image) override {
    return nullptr;
  }
  void DestroySharedImage(const SyncToken& sync_token,
                          const Mailbox& mailbox) override {}
  void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) override {}
  SwapChainSharedImages CreateSwapChain(viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        uint32_t usage) override {
    return {nullptr, nullptr};
  }
  void PresentSwapChain(const SyncToken& sync_token,
                        const Mailbox& mailbox) override {}
#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override {}
#endif  // BUILDFLAG(IS_FUCHSIA)
  SyncToken GenVerifiedSyncToken() override { return SyncToken(); }
  SyncToken GenUnverifiedSyncToken() override { return SyncToken(); }
  void VerifySyncToken(SyncToken& sync_token) override {}
  void WaitSyncToken(const SyncToken& sync_token) override {}
  void Flush() override {}
  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const Mailbox& mailbox) override {
    return nullptr;
  }
  const SharedImageCapabilities& GetCapabilities() override {
    return shared_image_capabilities_;
  }
  void SetCapabilities(const SharedImageCapabilities& caps) {}

  const Mailbox& GetMailboxForMostRecentlyCreatedSharedImage() {
    return mailbox_for_most_recently_created_shared_image_;
  }

 private:
  ~TestSharedImageInterface() override = default;

  SharedImageCapabilities shared_image_capabilities_;
  Mailbox mailbox_for_most_recently_created_shared_image_;
};

}  // namespace

TEST(ClientSharedImageTest, ImportUnowned) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  auto client_si = ClientSharedImage::ImportUnowned(
      ExportedSharedImage(mailbox, metadata, SyncToken(), GL_TEXTURE_2D));

  // Check that the ClientSI's state matches the input parameters.
  EXPECT_TRUE(client_si->mailbox() == mailbox);
  EXPECT_EQ(client_si->format(), kFormat);
  EXPECT_EQ(client_si->size(), kSize);
  EXPECT_EQ(client_si->usage(), kUsage);
  EXPECT_EQ(client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
  EXPECT_FALSE(client_si->HasHolder());
}

TEST(ClientSharedImageTest, CreateViaSharedImageInterface) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageInfo si_info{kFormat,
                          kSize,
                          gfx::ColorSpace(),
                          kTopLeft_GrSurfaceOrigin,
                          kOpaque_SkAlphaType,
                          kUsage,
                          ""};

  auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);

  EXPECT_TRUE(client_si->HasHolder());

  // Check that the ClientSI's state matches the input parameters.
  EXPECT_TRUE(client_si->mailbox() ==
              sii->GetMailboxForMostRecentlyCreatedSharedImage());
  EXPECT_EQ(client_si->format(), kFormat);
  EXPECT_EQ(client_si->size(), kSize);
  EXPECT_EQ(client_si->usage(), kUsage);

  // With no scanout or WebGPU usage, external sampling not configured, and no
  // client-side native buffer handle passed, the SharedImage should be using
  // the default texture target on all platforms.
  EXPECT_EQ(client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
}

TEST(ClientSharedImageTest, ExportAndImport) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageInfo si_info{kFormat,
                          kSize,
                          gfx::ColorSpace(),
                          kTopLeft_GrSurfaceOrigin,
                          kOpaque_SkAlphaType,
                          kUsage,
                          ""};

  auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);
  auto exported_si = client_si->Export();
  auto imported_client_si = ClientSharedImage::ImportUnowned(exported_si);

  EXPECT_TRUE(imported_client_si->mailbox() ==
              sii->GetMailboxForMostRecentlyCreatedSharedImage());
  EXPECT_EQ(imported_client_si->format(), kFormat);
  EXPECT_EQ(imported_client_si->size(), kSize);
  EXPECT_EQ(imported_client_si->usage(), kUsage);
  EXPECT_EQ(imported_client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
}

// The default target should be set for single-planar formats with no
// native buffer used.
TEST(ClientSharedImageTest,
     GetTextureTarget_SinglePlaneFormats_NoNativeBuffer) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  for (auto format : viz::SinglePlaneFormat::kAll) {
    SharedImageInfo si_info{format,
                            kSize,
                            gfx::ColorSpace(),
                            kTopLeft_GrSurfaceOrigin,
                            kOpaque_SkAlphaType,
                            kUsage,
                            ""};

    auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);
    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_2D));
  }
}

#if !BUILDFLAG(IS_MAC)
// When not on Mac, the default target should be used for multi-planar
// formats if external sampling is not set (the logic for Mac is distinct and is
// tested separately).
TEST(ClientSharedImageTest, GetTextureTarget_MultiplanarFormats) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Pass all the multiplanar formats that are used with hardware GMBs.
  for (auto format :
       {viz::MultiPlaneFormat::kYV12, viz::MultiPlaneFormat::kNV12,
        viz::MultiPlaneFormat::kNV12A, viz::MultiPlaneFormat::kP010}) {
    SharedImageInfo si_info{format,
                            kSize,
                            gfx::ColorSpace(),
                            kTopLeft_GrSurfaceOrigin,
                            kOpaque_SkAlphaType,
                            kUsage,
                            ""};

    auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);

    // Since the format does not have external sampling enabled, the default
    // target should be used.
    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_2D));
  }
}
#endif

#if BUILDFLAG(IS_OZONE)
// On Ozone, the target for native buffers should be used if a
// multiplanar format with external sampling is passed.
TEST(ClientSharedImageTest,
     GetTextureTarget_MultiplanarFormatsWithExternalSampling) {
  // For expedience, disable a CHECK in ClientSharedImage that external sampling
  // is used only if the client passed a native buffer.
  ClientSharedImage::AllowExternalSamplingWithoutNativeBuffersForTesting(true);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const gfx::Size kSize(256, 256);
  const uint32_t kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Pass all the multiplanar formats that are used with hardware GMBs.
  for (auto format :
       {viz::MultiPlaneFormat::kYV12, viz::MultiPlaneFormat::kNV12,
        viz::MultiPlaneFormat::kNV12A, viz::MultiPlaneFormat::kP010}) {
    format.SetPrefersExternalSampler();
    SharedImageInfo si_info{format,
                            kSize,
                            gfx::ColorSpace(),
                            kTopLeft_GrSurfaceOrigin,
                            kOpaque_SkAlphaType,
                            kUsage,
                            ""};

    auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);

    // Since the format has external sampling enabled, the platform-specific
    // target for native buffers should be used.
#if BUILDFLAG(IS_FUCHSIA)
    EXPECT_EQ(client_si->GetTextureTarget(), 0u);
#else
    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_EXTERNAL_OES));
#endif
  }

  ClientSharedImage::AllowExternalSamplingWithoutNativeBuffersForTesting(false);
}
#endif

}  // namespace gpu
