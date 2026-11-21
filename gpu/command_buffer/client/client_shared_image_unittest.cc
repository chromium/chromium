// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_context_support.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/exported_shared_image.mojom.h"
#include "gpu/ipc/common/exported_shared_image_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/win/scoped_handle.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#endif

namespace gpu {

namespace {

const gfx::Size kSize(256, 256);
constexpr viz::SharedImageFormat kMultiPlaneFormatsWithHardwareGMBs[4] = {
    viz::MultiPlaneFormat::kYV12, viz::MultiPlaneFormat::kNV12,
    viz::MultiPlaneFormat::kNV12A, viz::MultiPlaneFormat::kP010};

SharedImageInfo CreateSharedImageInfo(
    viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888,
    SharedImageUsageSet usage = SHARED_IMAGE_USAGE_RASTER_WRITE |
                                SHARED_IMAGE_USAGE_DISPLAY_READ) {
  return SharedImageInfo{format,
                         kSize,
                         gfx::ColorSpace(),
                         kTopLeft_GrSurfaceOrigin,
                         kOpaque_SkAlphaType,
                         usage,
                         "ClientSharedImageTest"};
}

class MockGLES2Interface : public gles2::GLES2InterfaceStub {
 public:
  MOCK_METHOD(void,
              WaitSyncTokenCHROMIUM,
              (const GLbyte* sync_token),
              (override));
  MOCK_METHOD(GLuint,
              CreateAndTexStorage2DSharedImageCHROMIUM,
              (const GLbyte* mailbox),
              (override));
  MOCK_METHOD(void,
              BeginSharedImageAccessDirectCHROMIUM,
              (GLuint texture, GLenum mode),
              (override));
  MOCK_METHOD(void,
              EndSharedImageAccessDirectCHROMIUM,
              (GLuint texture),
              (override));
  MOCK_METHOD(GLuint, CreateGpuFenceCHROMIUM, (), (override));
  MOCK_METHOD(void,
              DeleteTextures,
              (GLsizei n, const GLuint* textures),
              (override));
};

MATCHER_P(IsSyncToken, sync_token, "") {
  return *reinterpret_cast<const SyncToken*>(arg) == sync_token;
}

MATCHER_P(IsMailbox, mailbox, "") {
  return *reinterpret_cast<const Mailbox*>(arg) == mailbox;
}

}  // namespace

TEST(ClientSharedImageTest, ImportUnowned) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  auto client_si = ClientSharedImage::ImportUnowned(ExportedSharedImage(
      mailbox, metadata, SyncToken(), /*write_sync_token=*/SyncToken(),
      /*read_sync_tokens=*/{}, "ClientSharedImageTest", std::nullopt,
      std::nullopt, GL_TEXTURE_2D,
      /*is_software=*/false));

  // Check that the ClientSI's state matches the input parameters.
  EXPECT_EQ(client_si->mailbox(), mailbox);
  EXPECT_EQ(client_si->format(), kFormat);
  EXPECT_EQ(client_si->size(), kSize);
  EXPECT_EQ(client_si->usage(), kUsage);
  EXPECT_EQ(client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
  EXPECT_FALSE(client_si->HasHolder());
}

TEST(ClientSharedImageTest,
     ExportedSharedImageMojoDeserialization_TextureTargetZero) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), /*write_sync_token=*/SyncToken(),
      /*read_sync_tokens=*/{}, "ClientSharedImageTest", std::nullopt,
      std::nullopt,
      /*texture_target=*/0, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_TRUE(success);
  EXPECT_EQ(deserialized_si.texture_target_, 0u);
#else
  EXPECT_FALSE(success);
#endif
}

TEST(ClientSharedImageTest,
     ExportedSharedImageMojoDeserialization_EmptyBuffer) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  gfx::GpuMemoryBufferHandle empty_handle;
  empty_handle.type = gfx::EMPTY_BUFFER;

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), /*write_sync_token=*/SyncToken(),
      /*read_sync_tokens=*/{}, "ClientSharedImageTest", std::move(empty_handle),
      gfx::BufferUsage::GPU_READ,
      /*texture_target=*/GL_TEXTURE_2D, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

  EXPECT_FALSE(success);
}

TEST(ClientSharedImageTest,
     ExportedSharedImageMojoDeserialization_ZeroMailbox) {
  gpu::Mailbox mailbox;
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), /*write_sync_token=*/SyncToken(),
      /*read_sync_tokens=*/{}, "ClientSharedImageTest", std::nullopt,
      std::nullopt,
      /*texture_target=*/GL_TEXTURE_2D, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

  EXPECT_FALSE(success);
}

TEST(ClientSharedImageTest,
     ExportedSharedImageMojoDeserialization_DuplicateManagedSyncTokens) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id = CommandBufferId::FromUnsafeValue(42);

  // Two sync tokens from the same sequence (same client_id)
  SyncToken token1(ns, cmd_id, /*release_count=*/100);
  token1.SetVerifyFlush();
  SyncToken token2(ns, cmd_id, /*release_count=*/200);
  token2.SetVerifyFlush();

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), /*write_sync_token=*/SyncToken(),
      {token1, token2}, "ClientSharedImageTest", std::nullopt, std::nullopt,
      /*texture_target=*/GL_TEXTURE_2D, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

  // Deserialization must fail because duplicate client IDs are not allowed.
  EXPECT_FALSE(success);
}

TEST(ClientSharedImageTest,
     ExportedSharedImageMojoDeserialization_ValidManagedSyncTokens) {
  auto mailbox = Mailbox::Generate();
  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId write_cmd_id = CommandBufferId::FromUnsafeValue(41);
  CommandBufferId cmd_id1 = CommandBufferId::FromUnsafeValue(42);
  CommandBufferId cmd_id2 = CommandBufferId::FromUnsafeValue(43);

  SyncToken write_token(ns, write_cmd_id, /*release_count=*/50);
  write_token.SetVerifyFlush();
  SyncToken token1(ns, cmd_id1, /*release_count=*/100);
  token1.SetVerifyFlush();
  SyncToken token2(ns, cmd_id2, /*release_count=*/200);
  token2.SetVerifyFlush();

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), write_token, {token1, token2},
      "ClientSharedImageTest", std::nullopt, std::nullopt,
      /*texture_target=*/GL_TEXTURE_2D, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

  EXPECT_TRUE(success);
  EXPECT_EQ(deserialized_si.write_sync_token_, write_token);
  EXPECT_EQ(deserialized_si.read_sync_tokens_.size(), 2u);
  EXPECT_EQ(deserialized_si.read_sync_tokens_[0], token1);
  EXPECT_EQ(deserialized_si.read_sync_tokens_[1], token2);
}

TEST(ClientSharedImageTest, CreateMappableBufferFromHandle_EmptyBuffer) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  SharedImageMetadata metadata{kFormat,
                               kSize,
                               gfx::ColorSpace(),
                               kTopLeft_GrSurfaceOrigin,
                               kOpaque_SkAlphaType,
                               kUsage};

  gfx::GpuMemoryBufferHandle empty_handle;
  empty_handle.type = gfx::EMPTY_BUFFER;

  GpuMemoryBufferHandleInfo handle_info(std::move(empty_handle),
                                        gfx::BufferUsage::GPU_READ);

  // Creating a ClientSharedImage with an EMPTY_BUFFER handle should not crash.
  // The handle will be passed to CreateMappableBufferFromHandle which will
  // return nullptr, and the CHECK(mappable_buffer_) will fail, so we expect a
  // crash in death tests if we were to proceed, but since it's a CHECK, we can
  // test it with EXPECT_DEATH_IF_SUPPORTED.
  EXPECT_DEATH_IF_SUPPORTED(
      base::MakeRefCounted<ClientSharedImage>(
          Mailbox::Generate(), SharedImageInfo{metadata, "TestLabel"},
          SyncToken(), std::move(handle_info),
          base::MakeRefCounted<SharedImageInterfaceHolder>(sii.get())),
      "");
}

TEST(ClientSharedImageTest, CreateViaSharedImageInterface) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

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

TEST(ClientSharedImageTest, BackingWasExternallyUpdatedForwardsToSII) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  ASSERT_EQ(0u, sii->num_update_shared_image_no_fence_calls());
  client_si->BackingWasExternallyUpdated(gpu::SyncToken());
  EXPECT_EQ(1u, sii->num_update_shared_image_no_fence_calls());
}

// Verifies that invoking BackingWasExternallyUpdated() on a
// ClientSharedImage after its SharedImageInterface has been lost does not cause
// a crash.
TEST(ClientSharedImageTest, BackingWasExternallyUpdatedAfterLossOfSII) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  sii.reset();
  client_si->BackingWasExternallyUpdated(gpu::SyncToken());
}

TEST(ClientSharedImageTest, ExportAndImport) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);
  auto exported_si = client_si->Export();
  auto imported_client_si =
      ClientSharedImage::ImportUnowned(std::move(exported_si));

  EXPECT_EQ(imported_client_si->mailbox(), client_si->mailbox());
  EXPECT_EQ(imported_client_si->format(), kFormat);
  EXPECT_EQ(imported_client_si->size(), kSize);
  EXPECT_EQ(imported_client_si->usage(), kUsage);
  EXPECT_EQ(imported_client_si->GetTextureTarget(),
            static_cast<uint32_t>(GL_TEXTURE_2D));
}

#if BUILDFLAG(IS_WIN)
// An imported DXGI image without a shared memory pool, as
// TestSharedImageInterface and ImportUnowned() produce, cannot read pixels
// back from the GPU. It must still serve premapped mappings, and fail any
// other mapping without crashing.
TEST(ClientSharedImageTest, ImportedDXGIImageWithoutPoolMapsOnlyPremapped) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  constexpr uint8_t kMarker = 0x5a;

  // Attach a premapped region to a stand-in DXGI handle, as video capture does.
  auto region = base::UnsafeSharedMemoryRegion::Create(kSize.GetArea() * 3 / 2);
  ASSERT_TRUE(region.IsValid());
  auto region_mapping = region.Map();
  for (auto& byte : region_mapping.GetMemoryAsSpan<uint8_t>()) {
    byte = kMarker;
  }
  gfx::DXGIHandle dxgi_handle(base::win::ScopedHandle(::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)));
  ASSERT_TRUE(dxgi_handle.IsValid());
  dxgi_handle = dxgi_handle.CloneWithRegion(std::move(region));

  auto client_si = sii->CreateSharedImage(
      CreateSharedImageInfo(viz::MultiPlaneFormat::kNV12,
                            SHARED_IMAGE_USAGE_DISPLAY_READ),
      kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::GpuMemoryBufferHandle(std::move(dxgi_handle)));
  ASSERT_TRUE(client_si);
  auto imported_si =
      sii->ImportSharedImage(client_si->Export(/*with_buffer_handle=*/true));
  ASSERT_EQ(imported_si->GetGpuMemoryBufferType(), gfx::DXGI_SHARED_HANDLE);

  // Not premapped: this needs a readback, which needs a pool.
  imported_si->SetUsePreMappedMemory(false);
  EXPECT_FALSE(imported_si->Map());

  // Premapped: served from the region that came with the handle.
  imported_si->SetUsePreMappedMemory(true);
  auto scoped_mapping = imported_si->Map();
  ASSERT_TRUE(scoped_mapping);
  EXPECT_EQ(scoped_mapping->GetMemoryForPlane(0)[0], kMarker);
}
#endif

TEST(ClientSharedImageTest, MakeUnowned) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const auto kFormat = viz::SinglePlaneFormat::kRGBA_8888;
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);
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
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  for (auto format : viz::SinglePlaneFormat::kAll) {
    auto client_si = sii->CreateSharedImage(
        CreateSharedImageInfo(format, kUsage), kNullSurfaceHandle);
    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_2D));
  }
}

// When the client provides a native buffer with a single-plane format,
// GL_TEXTURE_2D
TEST(ClientSharedImageTest,
     GetTextureTarget_SinglePlaneFormats_ClientNativeBuffer) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  sii->emulate_client_provided_native_buffer();

  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  for (auto format : viz::SinglePlaneFormat::kAll) {
    auto client_si = sii->CreateSharedImage(
        CreateSharedImageInfo(format, kUsage), kNullSurfaceHandle);

    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_2D));
  }
}

// On all platforms, the default target should be used for multi-planar
// formats if external sampling is not set and scanout/WebGPU usage are not
// specified.
TEST(ClientSharedImageTest, GetTextureTarget_MultiplanarFormats) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Pass all the multiplanar formats that are used with hardware GMBs.
  for (auto format : kMultiPlaneFormatsWithHardwareGMBs) {
    auto client_si = sii->CreateSharedImage(
        CreateSharedImageInfo(format, kUsage), kNullSurfaceHandle);

    // Since the format does not have external sampling enabled, the default
    // target should be used.
    EXPECT_EQ(client_si->GetTextureTarget(),
              static_cast<uint32_t>(GL_TEXTURE_2D));
  }
}

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_ANDROID)
// On Ozone/Android, the target for native buffers should be used if a
// multiplanar format with external sampling is passed.
TEST(ClientSharedImageTest,
     GetTextureTarget_MultiplanarFormatsWithExternalSampling) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  sii->emulate_client_provided_native_buffer();

  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Pass all the multiplanar formats that are used with hardware GMBs.
  for (auto format :
       {viz::MultiPlaneFormat::kYV12, viz::MultiPlaneFormat::kNV12,
        viz::MultiPlaneFormat::kNV12A, viz::MultiPlaneFormat::kP010}) {
    format.SetPrefersExternalSampler();
    auto client_si = sii->CreateSharedImage(
        CreateSharedImageInfo(format, kUsage), kNullSurfaceHandle);

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

TEST(ClientSharedImageTest,
     MultiplanarFormatWithExternalSamplerSharedMemoryHandle) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();

  const SharedImageUsageSet kUsage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto format = viz::MultiPlaneFormat::kNV12;
  format.SetPrefersExternalSampler();
  SharedImageInfo si_info(format, gfx::Size(100, 100), gfx::ColorSpace(),
                          kUsage, "TestClearExternalSamplerHandle");
  EXPECT_EQ(si_info.alpha_type, kPremul_SkAlphaType);

  gfx::GpuMemoryBufferHandle handle =
      TestSharedImageInterface::CreateGMBHandle(format, gfx::Size(100, 100));
  auto client_si =
      sii->CreateSharedImage(si_info, kNullSurfaceHandle,
                             gfx::BufferUsage::GPU_READ, std::move(handle));
  ASSERT_TRUE(client_si);
  EXPECT_FALSE(client_si->format().PrefersExternalSampler());
  EXPECT_EQ(client_si->alpha_type(), kUnpremul_SkAlphaType);
}
#endif

// Checks whether ClientSharedImage correctly ignores input SyncTokens from
// clients and only output empty SyncTokens to clients when feature
// UseAutomaticSyncTokenManagement is enabled.
TEST(ClientSharedImageTest,
     AutomaticSyncTokenManagement_DeprecateInputAndOutputSyncTokens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  // 1. Passing an external SyncToken to BackingWasExternallyUpdated should
  // return an empty SyncToken to external callers when feature is enabled.
  SyncToken dummy_input_token(CommandBufferNamespace::GPU_IO,
                              CommandBufferId::FromUnsafeValue(123),
                              /*release_count=*/456);
  SyncToken returned_token =
      client_si->BackingWasExternallyUpdated(dummy_input_token);
  EXPECT_FALSE(returned_token.HasData());

  // 2. Exporting and ending export should produce an empty SyncToken to
  // external callers.
  auto exported_result = client_si->EndImport(dummy_input_token);
  SyncToken exported_token = client_si->EndExport(std::move(exported_result));
  EXPECT_FALSE(exported_token.HasData());

  // 3. EndExportAsVector should return an empty vector to external callers.
  auto exported_vec_result =
      client_si->EndImport(std::vector<SyncToken>{dummy_input_token});
  std::vector<SyncToken> exported_vec =
      client_si->EndExportAsVector(std::move(exported_vec_result));
  EXPECT_TRUE(exported_vec.empty());
}

TEST(ClientSharedImageTest,
     AutomaticSyncTokenManagement_EndDisplayCompositorAccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  SyncToken compositor_token(CommandBufferNamespace::GPU_IO,
                             CommandBufferId::FromUnsafeValue(789),
                             /*release_count=*/101);

  client_si->EndDisplayCompositorAccess(compositor_token);

  auto exported_result = client_si->EndImport(SyncToken());
  SyncToken expected_creation_token = client_si->creation_sync_token();
  expected_creation_token.SetVerifyFlush();
  SyncToken expected_compositor_token = compositor_token;
  expected_compositor_token.SetVerifyFlush();
  EXPECT_TRUE(exported_result.IsEqualForTesting(
      SharedImageExportResult::CreateForTesting(
          {expected_creation_token, expected_compositor_token})));
}

// Checks whether ClientSharedImage correctly stores only the latest SyncToken
// on a sequence when feature UseAutomaticSyncTokenManagement is enabled.
TEST(ClientSharedImageTest, AutomaticSyncTokenManagement_SyncTokenUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id = CommandBufferId::FromUnsafeValue(42);

  // Simulate storing tokens with increasing release counts on sequence (ns,
  // cmd_id).
  SyncToken token1(ns, cmd_id, /*release_count=*/100);
  SyncToken token2(ns, cmd_id, /*release_count=*/200);  // Newer
  SyncToken token3(ns, cmd_id, /*release_count=*/150);  // Older than token2

  // EndExport unpacks SharedImageExportResult and invokes
  // StoreSyncTokenInternal.
  client_si->EndExport(SharedImageExportResult::CreateForTesting(token1));
  client_si->EndExport(SharedImageExportResult::CreateForTesting(token2));
  client_si->EndExport(SharedImageExportResult::CreateForTesting(token3));

  // EndImport exports internal tokens and verifies unverified tokens.
  SharedImageExportResult export_result = client_si->EndImport(SyncToken());
  EXPECT_TRUE(export_result.HasData());

  // The export_result should contain the creation sync token and token2 (both
  // verified).
  SyncToken expected_creation_token = client_si->creation_sync_token();
  expected_creation_token.SetVerifyFlush();
  SyncToken expected_token = token2;
  expected_token.SetVerifyFlush();
  EXPECT_TRUE(
      export_result.IsEqualForTesting(SharedImageExportResult::CreateForTesting(
          {expected_creation_token, expected_token})));
}

TEST(ClientSharedImageTest, SignalLatestSyncToken_WithCallbackId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kUseAutomaticSyncTokenManagement);
  base::test::SingleThreadTaskEnvironment task_environment;
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id = CommandBufferId::FromUnsafeValue(42);
  SyncToken token(ns, cmd_id, /*release_count=*/100);

  // 1. Invalid sync token (release_count == 0): callback runs immediately.
  bool callback_called = false;
  uint64_t callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, {SyncToken()},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), /*context_support=*/nullptr, /*pending_callback_id=*/0);
  EXPECT_EQ(callback_id, 0u);
  EXPECT_TRUE(callback_called);

  // 2. Redundant callback (callback_id == pending_callback_id): callback not
  // run.
  callback_called = false;
  callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, {token},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), /*context_support=*/nullptr, /*pending_callback_id=*/100);
  EXPECT_EQ(callback_id, 100u);
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(callback_called);

  // 3. New callback (callback_id != pending_callback_id): callback runs via
  // SII.
  callback_called = false;
  callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, {token},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), /*context_support=*/nullptr, /*pending_callback_id=*/0);
  EXPECT_EQ(callback_id, 100u);
  EXPECT_FALSE(callback_called);
  base::RunLoop run_loop2;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(callback_called);
}

TEST(ClientSharedImageTest,
     SignalLatestSyncToken_WithCallbackId_AutomaticSyncTokenManagement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);
  base::test::SingleThreadTaskEnvironment task_environment;

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id = CommandBufferId::FromUnsafeValue(42);
  SyncToken token(ns, cmd_id, /*release_count=*/250);
  client_si->EndExport(SharedImageExportResult::CreateForTesting(token));

  viz::TestContextSupport context_support;
  context_support.set_sync_point_client_id(token.GetClientId());

  bool callback_called = false;
  uint64_t callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, /*sync_tokens=*/{},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), &context_support, /*pending_callback_id=*/0);
  EXPECT_EQ(callback_id, 250u);
  EXPECT_FALSE(callback_called);
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST(ClientSharedImageTest,
     SignalLatestSyncToken_Batch_AutomaticSyncTokenManagement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);
  base::test::SingleThreadTaskEnvironment task_environment;

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si1 =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);
  auto client_si2 =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id1 = CommandBufferId::FromUnsafeValue(10);
  CommandBufferId cmd_id2 = CommandBufferId::FromUnsafeValue(20);
  SyncToken token1(ns, cmd_id1, /*release_count=*/100);
  SyncToken token2(ns, cmd_id2, /*release_count=*/200);

  client_si1->EndExport(SharedImageExportResult::CreateForTesting(token1));
  client_si2->EndExport(SharedImageExportResult::CreateForTesting(token2));

  bool callback_called = false;
  ClientSharedImage::SignalLatestSyncToken(
      {client_si1, client_si2}, /*sync_tokens=*/{},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get());
  EXPECT_FALSE(callback_called);
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

TEST(ClientSharedImageTest, DestroySharedImage_AutomaticSyncTokenManagement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si =
      sii->CreateSharedImage(CreateSharedImageInfo(), kNullSurfaceHandle);
  SyncToken creation_token = client_si->creation_sync_token();

  CommandBufferNamespace ns = CommandBufferNamespace::GPU_IO;
  CommandBufferId cmd_id1 = CommandBufferId::FromUnsafeValue(10);
  CommandBufferId cmd_id2 = CommandBufferId::FromUnsafeValue(20);
  SyncToken token1(ns, cmd_id1, /*release_count=*/100);
  SyncToken token2(ns, cmd_id2, /*release_count=*/200);

  client_si->EndExport(SharedImageExportResult::CreateForTesting(token1));
  // In automatic mode, UpdateDestructionSyncToken is effectively a no-op and
  // any SyncToken passed into it should be ignored.
  client_si->UpdateDestructionSyncToken(token2);
  client_si->UpdateDestructionSyncToken(SyncToken());

  client_si.reset();

  EXPECT_THAT(sii->MostRecentDestroyTokens(),
              testing::UnorderedElementsAre(creation_token, token1));
}

// A fence created on the GL context alone doesn't cover writes made outside
// it, e.g. by WebGPU. Check that the fence follows a GL read access to each
// shared image, which waits on the fence such a writer leaves on the image.
TEST(ClientSharedImageTest, CreateGpuFenceForSyncTokensWaitsForWrites) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const SharedImageInfo info = CreateSharedImageInfo(
      viz::SinglePlaneFormat::kRGBA_8888,
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  std::vector<scoped_refptr<ClientSharedImage>> shared_images = {
      sii->CreateSharedImage(info, kNullSurfaceHandle),
      sii->CreateSharedImage(info, kNullSurfaceHandle)};
  const SyncToken token(CommandBufferNamespace::GPU_IO,
                        CommandBufferId::FromUnsafeValue(42),
                        /*release_count=*/100);

  testing::NiceMock<MockGLES2Interface> gl;
  viz::TestContextSupport context_support;
  // Creating and accessing the textures waits on other SyncTokens too.
  EXPECT_CALL(gl, WaitSyncTokenCHROMIUM(testing::_))
      .Times(testing::AnyNumber());
  testing::Sequence sequence;
  EXPECT_CALL(gl, WaitSyncTokenCHROMIUM(IsSyncToken(token)))
      .InSequence(sequence);
  GLuint texture_id = 10;
  for (const auto& shared_image : shared_images) {
    ++texture_id;
    EXPECT_CALL(gl, CreateAndTexStorage2DSharedImageCHROMIUM(
                        IsMailbox(shared_image->mailbox())))
        .InSequence(sequence)
        .WillOnce(testing::Return(texture_id));
    EXPECT_CALL(gl, BeginSharedImageAccessDirectCHROMIUM(
                        texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM))
        .InSequence(sequence);
    EXPECT_CALL(gl, EndSharedImageAccessDirectCHROMIUM(texture_id))
        .InSequence(sequence);
  }
  EXPECT_CALL(gl, CreateGpuFenceCHROMIUM())
      .InSequence(sequence)
      .WillOnce(testing::Return(1));

  SharedImageFenceHelper fence_helper(&gl);
  ClientSharedImage::CreateGpuFenceForSyncTokens(
      shared_images, {token}, &fence_helper, &context_support,
      base::DoNothing());
}

// The textures that CreateGpuFenceForSyncTokens() accesses are created once per
// shared image, and deleted once only the helper references the shared image.
TEST(ClientSharedImageTest, CreateGpuFenceForSyncTokensReusesTextures) {
  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  const SharedImageInfo info = CreateSharedImageInfo(
      viz::SinglePlaneFormat::kRGBA_8888,
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  scoped_refptr<ClientSharedImage> image1 =
      sii->CreateSharedImage(info, kNullSurfaceHandle);
  scoped_refptr<ClientSharedImage> image2 =
      sii->CreateSharedImage(info, kNullSurfaceHandle);
  const Mailbox mailbox1 = image1->mailbox();

  testing::NiceMock<MockGLES2Interface> gl;
  viz::TestContextSupport context_support;
  SharedImageFenceHelper fence_helper(&gl);
  auto create_fence = [&](scoped_refptr<ClientSharedImage> shared_image) {
    ClientSharedImage::CreateGpuFenceForSyncTokens(
        {std::move(shared_image)}, {}, &fence_helper, &context_support,
        base::DoNothing());
  };

  // Each image gets one texture, however often and in whichever order it is
  // fenced.
  EXPECT_CALL(gl, CreateAndTexStorage2DSharedImageCHROMIUM(IsMailbox(mailbox1)))
      .WillOnce(testing::Return(11));
  EXPECT_CALL(gl, CreateAndTexStorage2DSharedImageCHROMIUM(
                      IsMailbox(image2->mailbox())))
      .WillOnce(testing::Return(12));
  EXPECT_CALL(gl, BeginSharedImageAccessDirectCHROMIUM(
                      11, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM))
      .Times(2);
  EXPECT_CALL(gl, BeginSharedImageAccessDirectCHROMIUM(
                      12, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM))
      .Times(2);
  EXPECT_CALL(gl, DeleteTextures(testing::_, testing::_)).Times(0);
  create_fence(image1);
  create_fence(image2);
  create_fence(image1);
  create_fence(image2);
  testing::Mock::VerifyAndClearExpectations(&gl);

  // Once the owner drops `image1` (e.g. its swapchain was recreated), the next
  // fence deletes its texture, which releases the shared image, and a new
  // image gets a new texture.
  image1.reset();
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox1));
  scoped_refptr<ClientSharedImage> image3 =
      sii->CreateSharedImage(info, kNullSurfaceHandle);
  EXPECT_CALL(gl, CreateAndTexStorage2DSharedImageCHROMIUM(
                      IsMailbox(image3->mailbox())))
      .WillOnce(testing::Return(13));
  EXPECT_CALL(gl, DeleteTextures(1, testing::Pointee(11)));
  create_fence(image3);
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox1));
  testing::Mock::VerifyAndClearExpectations(&gl);

  // Destroying the helper deletes the remaining textures.
  EXPECT_CALL(gl, DeleteTextures(1, testing::Pointee(12)));
  EXPECT_CALL(gl, DeleteTextures(1, testing::Pointee(13)));
}

TEST(ClientSharedImageTest,
     AutomaticSyncTokenManagement_SeparateReadAndWriteSyncTokens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si = sii->CreateSharedImage(
      CreateSharedImageInfo(viz::SinglePlaneFormat::kRGBA_8888,
                            SHARED_IMAGE_USAGE_RASTER_READ |
                                SHARED_IMAGE_USAGE_RASTER_WRITE |
                                SHARED_IMAGE_USAGE_DISPLAY_READ),
      kNullSurfaceHandle);
  SyncToken creation_token = client_si->creation_sync_token();

  constexpr uint64_t kReader1Id = 10;
  constexpr uint64_t kReader2Id = 20;
  constexpr uint64_t kWriterId = 30;
  viz::TestRasterInterface reader1_ri;
  reader1_ri.set_command_buffer_id(kReader1Id);
  viz::TestRasterInterface reader2_ri;
  reader2_ri.set_command_buffer_id(kReader2Id);
  viz::TestRasterInterface writer_ri;
  writer_ri.set_command_buffer_id(kWriterId);

  // 1. First read-only access waits on `creation_token` (a write token).
  {
    auto read_access1 = client_si->BeginRasterAccess(&reader1_ri, SyncToken(),
                                                     /*readonly=*/true);
    RasterScopedAccess::EndAccess(std::move(read_access1));
  }
  EXPECT_THAT(reader1_ri.waited_sync_tokens(),
              testing::UnorderedElementsAre(creation_token));
  SyncToken reader1_token(CommandBufferNamespace::GPU_IO,
                          CommandBufferId::FromUnsafeValue(kReader1Id),
                          /*release_count=*/1);

  // 2. Second read-only access only waits on write tokens (`creation_token`),
  // and does NOT wait on `reader1_token`.
  {
    auto read_access2 = client_si->BeginRasterAccess(&reader2_ri, SyncToken(),
                                                     /*readonly=*/true);
    RasterScopedAccess::EndAccess(std::move(read_access2));
  }
  EXPECT_THAT(reader2_ri.waited_sync_tokens(),
              testing::UnorderedElementsAre(creation_token));
  SyncToken reader2_token(CommandBufferNamespace::GPU_IO,
                          CommandBufferId::FromUnsafeValue(kReader2Id),
                          /*release_count=*/1);

  // 3. Write access waits on all prior read and write tokens, then clears the
  // read SyncToken map and replaces the write SyncToken with `writer_token1`.
  {
    auto write_access = client_si->BeginRasterAccess(&writer_ri, SyncToken(),
                                                     /*readonly=*/false);
    RasterScopedAccess::EndAccess(std::move(write_access));
  }
  EXPECT_THAT(writer_ri.waited_sync_tokens(),
              testing::UnorderedElementsAre(creation_token, reader1_token,
                                            reader2_token));
  SyncToken writer_token1(CommandBufferNamespace::GPU_IO,
                          CommandBufferId::FromUnsafeValue(kWriterId),
                          /*release_count=*/1);

  // 4. Subsequent read-only access on `writer_ri` only waits on the latest
  // write token (`writer_token1`, which replaced `creation_token`), and
  // generates a newer read token on `kWriterId` (release_count = 2).
  writer_ri.ClearWaitedSyncTokens();
  {
    auto read_access3 = client_si->BeginRasterAccess(&writer_ri, SyncToken(),
                                                     /*readonly=*/true);
    RasterScopedAccess::EndAccess(std::move(read_access3));
  }
  EXPECT_THAT(writer_ri.waited_sync_tokens(),
              testing::UnorderedElementsAre(writer_token1));
  SyncToken writer_read_token2(CommandBufferNamespace::GPU_IO,
                               CommandBufferId::FromUnsafeValue(kWriterId),
                               /*release_count=*/2);

  // 5. Another read-only access on `reader1_ri` still only waits on
  // `writer_token1` (release_count = 1), whereas a write access on `reader1_ri`
  // waits on `reader1_token2` and the newer `writer_read_token2`
  // (release_count = 2) deduplicated on `kWriterId` (while `creation_token` and
  // `reader2_token` from before step 3's write were already cleared).
  reader1_ri.ClearWaitedSyncTokens();
  {
    auto read_access4 = client_si->BeginRasterAccess(&reader1_ri, SyncToken(),
                                                     /*readonly=*/true);
    RasterScopedAccess::EndAccess(std::move(read_access4));
  }
  EXPECT_THAT(reader1_ri.waited_sync_tokens(),
              testing::UnorderedElementsAre(writer_token1));
  SyncToken reader1_token2(CommandBufferNamespace::GPU_IO,
                           CommandBufferId::FromUnsafeValue(kReader1Id),
                           /*release_count=*/2);

  reader1_ri.ClearWaitedSyncTokens();
  {
    auto write_access2 = client_si->BeginRasterAccess(&reader1_ri, SyncToken(),
                                                      /*readonly=*/false);
    RasterScopedAccess::EndAccess(std::move(write_access2));
  }
  EXPECT_THAT(
      reader1_ri.waited_sync_tokens(),
      testing::UnorderedElementsAre(reader1_token2, writer_read_token2));
}

// Checks that when feature UseAutomaticSyncTokenManagement is enabled, creating
// a GL texture only waits on the creation SyncToken, whereas a subsequent read
// access waits on the latest write SyncToken.
TEST(ClientSharedImageTest,
     AutomaticSyncTokenManagement_CreateGLTextureWaitsOnCreationSyncToken) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si = sii->CreateSharedImage(
      CreateSharedImageInfo(
          viz::SinglePlaneFormat::kRGBA_8888,
          SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_GLES2_READ),
      kNullSurfaceHandle);
  SyncToken creation_token = client_si->creation_sync_token();
  ASSERT_TRUE(creation_token.HasData());

  // Write to the SharedImage so that the latest write SyncToken differs from
  // the creation SyncToken.
  constexpr uint64_t kWriterId = 30;
  viz::TestRasterInterface writer_ri;
  writer_ri.set_command_buffer_id(kWriterId);
  {
    auto write_access = client_si->BeginRasterAccess(&writer_ri, SyncToken(),
                                                     /*readonly=*/false);
    RasterScopedAccess::EndAccess(std::move(write_access));
  }
  SyncToken writer_token(CommandBufferNamespace::GPU_IO,
                         CommandBufferId::FromUnsafeValue(kWriterId),
                         /*release_count=*/1);

  // 1. Creating the GL texture only waits on `creation_token`.
  viz::TestGLES2Interface gl;
  auto texture = client_si->CreateGLTexture(&gl);
  EXPECT_THAT(gl.waited_sync_tokens(), testing::ElementsAre(creation_token));

  // 2. A read access to the pixels waits on the latest write SyncToken
  // (`writer_token`).
  gl.ClearWaitedSyncTokens();
  {
    auto read_access = texture->BeginAccess(SyncToken(), /*readonly=*/true);
    SharedImageTexture::ScopedAccess::EndAccess(std::move(read_access));
  }
  EXPECT_THAT(gl.waited_sync_tokens(), testing::ElementsAre(writer_token));
}

// Checks that when feature UseAutomaticSyncTokenManagement is enabled,
// BackingWasExternallyUpdated() does not wait on tracked SyncTokens via SII
// (TestSharedImageInterface::WaitSyncToken is NOTREACHED) and replaces all
// previously tracked read and write SyncTokens with the update's SyncToken.
TEST(ClientSharedImageTest,
     AutomaticSyncTokenManagement_BackingWasExternallyUpdatedReplacesTokens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseAutomaticSyncTokenManagement);

  auto sii = base::MakeRefCounted<TestSharedImageInterface>();
  auto client_si = sii->CreateSharedImage(
      CreateSharedImageInfo(viz::SinglePlaneFormat::kRGBA_8888,
                            SHARED_IMAGE_USAGE_RASTER_READ |
                                SHARED_IMAGE_USAGE_RASTER_WRITE |
                                SHARED_IMAGE_USAGE_DISPLAY_READ),
      kNullSurfaceHandle);

  constexpr uint64_t kCompositorId = 789;
  SyncToken compositor_token1(CommandBufferNamespace::GPU_IO,
                              CommandBufferId::FromUnsafeValue(kCompositorId),
                              /*release_count=*/101);
  SyncToken compositor_token2(CommandBufferNamespace::GPU_IO,
                              CommandBufferId::FromUnsafeValue(kCompositorId),
                              /*release_count=*/102);

  // 1. After an external update, prior write (`creation_token`) and read
  // (`compositor_token1`) SyncTokens are replaced by `update_token1`.
  client_si->EndDisplayCompositorAccess(compositor_token1);
  client_si->BackingWasExternallyUpdated(SyncToken());
  SyncToken update_token1 = sii->MostRecentGeneratedToken();

  viz::TestRasterInterface reader_ri;
  {
    auto read_access = client_si->BeginRasterAccess(&reader_ri, SyncToken(),
                                                    /*readonly=*/true);
    RasterScopedAccess::EndAccess(std::move(read_access));
  }
  EXPECT_THAT(reader_ri.waited_sync_tokens(),
              testing::ElementsAre(update_token1));

  // 2. Same for the GpuFence overload: `update_token2` replaces `update_token1`
  // and any subsequent read SyncTokens.
  client_si->EndDisplayCompositorAccess(compositor_token2);
  client_si->BackingWasExternallyUpdated(std::unique_ptr<gfx::GpuFence>());
  SyncToken update_token2 = sii->MostRecentGeneratedToken();

  // 3. Destruction only needs to wait on the SyncToken of the latest update.
  client_si.reset();
  EXPECT_THAT(sii->MostRecentDestroyTokens(),
              testing::ElementsAre(update_token2));
}

}  // namespace gpu
