// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/exported_shared_image.mojom.h"
#include "gpu/ipc/common/exported_shared_image_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"

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
      mailbox, metadata, SyncToken(), /*managed_sync_tokens=*/{},
      "ClientSharedImageTest", std::nullopt, std::nullopt, GL_TEXTURE_2D,
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
      mailbox, metadata, SyncToken(), /*managed_sync_tokens=*/{},
      "ClientSharedImageTest", std::nullopt, std::nullopt,
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
      mailbox, metadata, SyncToken(), /*managed_sync_tokens=*/{},
      "ClientSharedImageTest", std::move(empty_handle),
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
      mailbox, metadata, SyncToken(), /*managed_sync_tokens=*/{},
      "ClientSharedImageTest", std::nullopt, std::nullopt,
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
      mailbox, metadata, SyncToken(), {token1, token2}, "ClientSharedImageTest",
      std::nullopt, std::nullopt,
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
  CommandBufferId cmd_id1 = CommandBufferId::FromUnsafeValue(42);
  CommandBufferId cmd_id2 = CommandBufferId::FromUnsafeValue(43);

  SyncToken token1(ns, cmd_id1, /*release_count=*/100);
  token1.SetVerifyFlush();
  SyncToken token2(ns, cmd_id2, /*release_count=*/200);
  token2.SetVerifyFlush();

  ExportedSharedImage exported_si(
      mailbox, metadata, SyncToken(), {token1, token2}, "ClientSharedImageTest",
      std::nullopt, std::nullopt,
      /*texture_target=*/GL_TEXTURE_2D, /*is_software=*/false);

  ExportedSharedImage deserialized_si;
  bool success =
      mojo::test::SerializeAndDeserialize<gpu::mojom::ExportedSharedImage>(
          exported_si, deserialized_si);

  EXPECT_TRUE(success);
  EXPECT_EQ(deserialized_si.managed_sync_tokens_.size(), 2u);
  EXPECT_EQ(deserialized_si.managed_sync_tokens_[0], token1);
  EXPECT_EQ(deserialized_si.managed_sync_tokens_[1], token2);
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

#if BUILDFLAG(IS_OZONE)
// On Ozone, the target for native buffers should be used if a
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

  // The export_result should only contain token2 (verified).
  SyncToken expected_token = token2;
  expected_token.SetVerifyFlush();
  EXPECT_TRUE(export_result.IsEqualForTesting(expected_token));
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
      sii.get(), /*pending_callback_id=*/0);
  EXPECT_EQ(callback_id, 0u);
  EXPECT_TRUE(callback_called);

  // 2. Redundant callback (callback_id == pending_callback_id): callback not
  // run.
  callback_called = false;
  callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, {token},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), /*pending_callback_id=*/100);
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
      sii.get(), /*pending_callback_id=*/0);
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

  bool callback_called = false;
  uint64_t callback_id = ClientSharedImage::SignalLatestSyncToken(
      {client_si}, /*sync_tokens=*/{},
      base::BindOnce([](bool* called) { *called = true; }, &callback_called),
      sii.get(), /*pending_callback_id=*/0);
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

}  // namespace gpu
