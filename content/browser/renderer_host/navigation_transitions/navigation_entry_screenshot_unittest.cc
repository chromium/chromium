// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "cc/slim/texture_layer.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

gpu::SyncToken Verified(gpu::SyncToken token) {
  token.SetVerifyFlush();
  return token;
}

gpu::SyncToken SyncTokenFromPointer(const GLbyte* sync_token) {
  gpu::SyncToken sync_token_data;
  if (sync_token) {
    // SAFETY: Test only. Called from viz::TestRasterInterface.
    UNSAFE_BUFFERS(
        base::span(sync_token_data.GetData(), sizeof(sync_token_data))
            .copy_from(base::span(sync_token, sizeof(sync_token_data))));
  }
  return sync_token_data;
}

class ReadbackTestRasterInterface : public viz::TestRasterInterface {
 public:
  ReadbackTestRasterInterface() = default;
  ~ReadbackTestRasterInterface() override = default;

  void ReadbackARGBPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      GrSurfaceOrigin source_origin,
      const gfx::Size& source_size,
      const gfx::Point& source_starting_point,
      const SkImageInfo& dst_info,
      GLuint dst_row_bytes,
      base::span<uint8_t> out,
      base::OnceCallback<void(bool)> readback_done) override {
    readback_done_ = std::move(readback_done);
    readback_started_.SetValue();
  }

  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override {
    viz::TestRasterInterface::GenUnverifiedSyncTokenCHROMIUM(sync_token);
    gpu::SyncToken sync_token_data = SyncTokenFromPointer(sync_token);
    if (sync_token_data.HasData()) {
      generated_sync_tokens_.push_back(sync_token_data);
    }
  }

  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override {
    viz::TestRasterInterface::WaitSyncTokenCHROMIUM(sync_token);
    gpu::SyncToken sync_token_data = SyncTokenFromPointer(sync_token);
    if (sync_token_data.HasData()) {
      waited_sync_tokens_.push_back(sync_token_data);
    }
  }

  bool WaitForReadbackStarted() { return readback_started_.Wait(); }

  void CompleteReadback(bool success) {
    ASSERT_TRUE(readback_done_);
    std::move(readback_done_).Run(success);
  }

  const std::vector<gpu::SyncToken>& generated_sync_tokens() const {
    return generated_sync_tokens_;
  }

  const std::vector<gpu::SyncToken>& waited_sync_tokens() const {
    return waited_sync_tokens_;
  }

 private:
  base::test::TestFuture<void> readback_started_;
  base::OnceCallback<void(bool)> readback_done_;
  std::vector<gpu::SyncToken> generated_sync_tokens_;
  std::vector<gpu::SyncToken> waited_sync_tokens_;
};

class NavigationEntryScreenshotTest : public testing::Test {
 public:
  void SetUp() override {
    NavigationEntryScreenshot::SetDisableCompressionForTesting(true);
    auto raster_interface = std::make_unique<ReadbackTestRasterInterface>();
    raster_interface_ = raster_interface.get();
    context_provider_ =
        viz::TestContextProvider::CreateRaster(std::move(raster_interface));
    context_provider_->BindToCurrentSequence();
    shared_image_ = gpu::ClientSharedImage::CreateForTesting(
        gpu::SHARED_IMAGE_USAGE_RASTER_READ |
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);
  }

  void TearDown() override {
    NavigationEntryScreenshot::SetDisableCompressionForTesting(false);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  raw_ptr<ReadbackTestRasterInterface> raster_interface_ = nullptr;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
};

TEST_F(NavigationEntryScreenshotTest, DestroyedDuringReadBackWaitsForRead) {
  base::test::TestFuture<const gpu::SyncToken&, bool> release_future;
  auto provider = NavigationEntryScreenshot::SharedImageHolder::Create(
      context_provider_, shared_image_, release_future.GetCallback());

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      std::move(provider), NavigationTransitionData::UniqueId(1),
      /*supports_etc_non_power_of_two=*/true, base::NullCallback());
  ASSERT_TRUE(raster_interface_->WaitForReadbackStarted());
  ASSERT_EQ(raster_interface_->generated_sync_tokens().size(), 1u);
  gpu::SyncToken read_token = raster_interface_->generated_sync_tokens()[0];

  screenshot.reset();
  ASSERT_TRUE(release_future.IsReady());
  EXPECT_THAT(raster_interface_->waited_sync_tokens(), testing::IsEmpty());
  EXPECT_EQ(raster_interface_->generated_sync_tokens().size(), 1u);
  EXPECT_EQ(release_future.Get<0>(), Verified(read_token));
  EXPECT_FALSE(release_future.Get<1>());
}

TEST_F(NavigationEntryScreenshotTest,
       ReadBackBeforeTextureLayerReleaseCombinesSyncTokens) {
  base::test::TestFuture<const gpu::SyncToken&, bool> release_future;
  auto provider = NavigationEntryScreenshot::SharedImageHolder::Create(
      context_provider_, shared_image_, release_future.GetCallback());

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      provider, NavigationTransitionData::UniqueId(1),
      /*supports_etc_non_power_of_two=*/true, base::NullCallback());
  auto layer = screenshot->CreateTextureLayer();
  viz::TransferableResource resource;
  viz::ReleaseCallback layer_release_callback;
  ASSERT_TRUE(provider->PrepareTransferableResource(&resource,
                                                    &layer_release_callback));
  provider.reset();

  // Issue DoReadBack() before the texture layer's resource is returned.
  ASSERT_TRUE(raster_interface_->WaitForReadbackStarted());
  ASSERT_EQ(raster_interface_->generated_sync_tokens().size(), 1u);

  gpu::SyncToken viz_token(gpu::CommandBufferNamespace::GPU_IO,
                           gpu::CommandBufferId::FromUnsafeValue(99u), 1u);
  viz_token.SetVerifyFlush();
  std::move(layer_release_callback).Run(viz_token, /*is_lost=*/false);
  screenshot.reset();

  ASSERT_TRUE(release_future.IsReady());
  EXPECT_THAT(raster_interface_->waited_sync_tokens(),
              testing::ElementsAre(viz_token));
  ASSERT_EQ(raster_interface_->generated_sync_tokens().size(), 2u);
  gpu::SyncToken combined_token = raster_interface_->generated_sync_tokens()[1];
  EXPECT_EQ(release_future.Get<0>(), Verified(combined_token));
}

TEST_F(NavigationEntryScreenshotTest,
       CompletedReadBackClearsReadSyncTokenBeforeRelease) {
  base::test::TestFuture<const gpu::SyncToken&, bool> release_future;
  auto provider = NavigationEntryScreenshot::SharedImageHolder::Create(
      context_provider_, shared_image_, release_future.GetCallback());

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      provider, NavigationTransitionData::UniqueId(1),
      /*supports_etc_non_power_of_two=*/true, base::NullCallback());
  auto layer = screenshot->CreateTextureLayer();
  viz::TransferableResource resource;
  viz::ReleaseCallback layer_release_callback;
  ASSERT_TRUE(provider->PrepareTransferableResource(&resource,
                                                    &layer_release_callback));
  provider.reset();

  ASSERT_TRUE(raster_interface_->WaitForReadbackStarted());
  ASSERT_EQ(raster_interface_->generated_sync_tokens().size(), 1u);
  raster_interface_->CompleteReadback(/*success=*/true);

  gpu::SyncToken viz_token(gpu::CommandBufferNamespace::GPU_IO,
                           gpu::CommandBufferId::FromUnsafeValue(99u), 1u);
  viz_token.SetVerifyFlush();
  std::move(layer_release_callback).Run(viz_token, /*is_lost=*/false);
  screenshot.reset();

  ASSERT_TRUE(release_future.IsReady());
  EXPECT_THAT(raster_interface_->waited_sync_tokens(), testing::IsEmpty());
  EXPECT_EQ(raster_interface_->generated_sync_tokens().size(), 1u);
  EXPECT_EQ(release_future.Get<0>(), viz_token);
}

}  // namespace
}  // namespace content
