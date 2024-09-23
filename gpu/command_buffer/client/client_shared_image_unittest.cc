// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

namespace {

constexpr viz::SharedImageFormat kMultiPlaneFormatsWithHardwareGMBs[4] = {
    viz::MultiPlaneFormat::kYV12, viz::MultiPlaneFormat::kNV12,
    viz::MultiPlaneFormat::kNV12A, viz::MultiPlaneFormat::kP010};

}  // namespace

TEST(ClientSharedImageTest, ImportUnowned) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
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
  EXPECT_EQ(client_si->mailbox(), mailbox);
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
  const SharedImageUsageSet kUsage =
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
  EXPECT_FALSE(client_si->mailbox().IsZero());

  // Check that the ClientSI's state matches the input parameters.
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
  const SharedImageUsageSet kUsage =
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

  EXPECT_EQ(imported_client_si->mailbox(), client_si->mailbox());
  EXPECT_EQ(imported_client_si->format(), kFormat);
  EXPECT_EQ(imported_client_si->size(), kSize);
  EXPECT_EQ(imported_client_si->usage(), kUsage);
  EXPECT_EQ(imported_client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
}

TEST(ClientSharedImageTest, MakeUnowned) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageInfo si_info{kFormat,
                          kSize,
                          gfx::ColorSpace(),
                          kTopLeft_GrSurfaceOrigin,
                          kOpaque_SkAlphaType,
                          kUsage,
                          ""};

  auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);
  auto unowned_si = client_si->MakeUnowned();

  EXPECT_EQ(unowned_si->mailbox(), client_si->mailbox());
  EXPECT_EQ(unowned_si->format(), kFormat);
  EXPECT_EQ(unowned_si->size(), kSize);
  EXPECT_EQ(unowned_si->usage(), kUsage);
  EXPECT_EQ(unowned_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
  EXPECT_FALSE(unowned_si->HasHolder());
}

// The default target should be set for single-planar formats with no
// native buffer used.
TEST(ClientSharedImageTest,
     GetTextureTarget_SinglePlaneFormats_NoNativeBuffer) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
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

// When the client provides a native buffer with a single-plane format,
// GL_TEXTURE_2D should be used as the texture target on all platforms other
// than Mac, where the target for IO surfaces should be used.
TEST(ClientSharedImageTest,
     GetTextureTarget_SinglePlaneFormats_ClientNativeBuffer) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  sii->emulate_client_provided_native_buffer();

#if BUILDFLAG(IS_MAC)
  // Explicitly set the texture target for IO surfaces to a target other than
  // GL_TEXTURE_2D to ensure that the test is meaningful on Mac.
  const uint32_t kTargetForIOSurfaces = GL_TEXTURE_RECTANGLE_ARB;
  sii->set_texture_target_for_io_surfaces(kTargetForIOSurfaces);
#endif

  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
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

#if BUILDFLAG(IS_MAC)
    const uint32_t expected_texture_target = kTargetForIOSurfaces;
#else
    const uint32_t expected_texture_target = GL_TEXTURE_2D;
#endif
    EXPECT_EQ(client_si->GetTextureTarget(), expected_texture_target);
  }
}

// When the client asks for SCANOUT usage, GL_TEXTURE_2D should be used as the
// texture target on all platforms other than Mac, where the target for IO
// surfaces should be used.
TEST(ClientSharedImageTest, GetTextureTarget_ScanoutUsage) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

#if BUILDFLAG(IS_MAC)
  // Explicitly set the texture target for IO surfaces to a target other than
  // GL_TEXTURE_2D to ensure that the test is meaningful on Mac.
  const uint32_t kTargetForIOSurfaces = GL_TEXTURE_RECTANGLE_ARB;
  sii->set_texture_target_for_io_surfaces(kTargetForIOSurfaces);
#endif

  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage = SHARED_IMAGE_USAGE_SCANOUT;

  // Test all single-plane formats as well as multiplane formats for which
  // hardware GMBs are supported.
  std::vector<viz::SharedImageFormat> formats_to_test;
  for (auto format : viz::SinglePlaneFormat::kAll) {
    formats_to_test.push_back(format);
  }
  for (auto format : kMultiPlaneFormatsWithHardwareGMBs) {
    formats_to_test.push_back(format);
  }

  for (auto format : formats_to_test) {
    SharedImageInfo si_info{format,
                            kSize,
                            gfx::ColorSpace(),
                            kTopLeft_GrSurfaceOrigin,
                            kOpaque_SkAlphaType,
                            kUsage,
                            ""};

    auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);

#if BUILDFLAG(IS_MAC)
    const uint32_t expected_texture_target = kTargetForIOSurfaces;
#else
    const uint32_t expected_texture_target = GL_TEXTURE_2D;
#endif
    EXPECT_EQ(client_si->GetTextureTarget(), expected_texture_target);
  }
}

// When the client asks for WEBGPU usage, GL_TEXTURE_2D should be used as the
// texture target on all platforms other than Mac, where the target for IO
// surfaces should be used.
TEST(ClientSharedImageTest, GetTextureTarget_WebGPUUsage) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

#if BUILDFLAG(IS_MAC)
  // Explicitly set the texture target for IO surfaces to a target other than
  // GL_TEXTURE_2D to ensure that the test is meaningful on Mac.
  const uint32_t kTargetForIOSurfaces = GL_TEXTURE_RECTANGLE_ARB;
  sii->set_texture_target_for_io_surfaces(kTargetForIOSurfaces);
#endif

  // Test all single-plane formats as well as multiplane formats for which
  // hardware GMBs are supported.
  std::vector<viz::SharedImageFormat> formats_to_test;
  for (auto format : viz::SinglePlaneFormat::kAll) {
    formats_to_test.push_back(format);
  }
  for (auto format : kMultiPlaneFormatsWithHardwareGMBs) {
    formats_to_test.push_back(format);
  }

  for (SharedImageUsageSet webgpu_usage :
       {SHARED_IMAGE_USAGE_WEBGPU_READ, SHARED_IMAGE_USAGE_WEBGPU_WRITE}) {
    const gfx::Size kSize(256, 256);
    const SharedImageUsageSet kUsage = webgpu_usage;

    for (auto format : formats_to_test) {
      SharedImageInfo si_info{format,
                              kSize,
                              gfx::ColorSpace(),
                              kTopLeft_GrSurfaceOrigin,
                              kOpaque_SkAlphaType,
                              kUsage,
                              ""};

      auto client_si = sii->CreateSharedImage(si_info, kNullSurfaceHandle);

#if BUILDFLAG(IS_MAC)
      const uint32_t expected_texture_target = kTargetForIOSurfaces;
#else
      const uint32_t expected_texture_target = GL_TEXTURE_2D;
#endif
      EXPECT_EQ(client_si->GetTextureTarget(), expected_texture_target);
    }
  }
}

// On all platforms, the default target should be used for multi-planar
// formats if external sampling is not set and scanout/WebGPU usage are not
// specified.
TEST(ClientSharedImageTest,
     GetTextureTarget_MultiplanarFormats_NoScanoutOrWebGPUUsage) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Pass all the multiplanar formats that are used with hardware GMBs.
  for (auto format : kMultiPlaneFormatsWithHardwareGMBs) {
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

#if BUILDFLAG(IS_OZONE)
// On Ozone, the target for native buffers should be used if a
// multiplanar format with external sampling is passed.
TEST(ClientSharedImageTest,
     GetTextureTarget_MultiplanarFormatsWithExternalSampling) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  sii->emulate_client_provided_native_buffer();

  const gfx::Size kSize(256, 256);
  const SharedImageUsageSet kUsage =
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
}
#endif

}  // namespace gpu
