// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_cache.h"

#include <array>

#include "base/test/task_environment.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

namespace blink {

class WebGpuSharedImageCacheTest : public testing::Test {
 public:
  WebGpuSharedImageCacheTest() = default;
  ~WebGpuSharedImageCacheTest() override = default;

  // Implements testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<WebGpuSharedImageCache> cache_;
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

void WebGpuSharedImageCacheTest::SetUp() {
  Platform::SetMainThreadTaskRunnerForTesting();
  test_context_provider_ = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContext(test_context_provider_.get(),
                             &image_decode_cache_);

  cache_ = std::make_unique<WebGpuSharedImageCache>(
      SharedGpuContext::ContextProviderWrapper(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebGpuSharedImageCacheTest::TearDown() {
  Platform::UnsetMainThreadTaskRunnerForTesting();
  SharedGpuContext::Reset();
}

TEST_F(WebGpuSharedImageCacheTest, MRUSameSize) {
  auto size = gfx::Size(10, 10);
  Vector<gpu::ClientSharedImage*> returned_shared_images;

  std::unique_ptr<WebGpuSharedImageLease> lease_0 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_0->GetSharedImage().get());

  std::unique_ptr<WebGpuSharedImageLease> lease_1 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_1->GetSharedImage().get());

  // Now release the leases to recycle the shared images.
  lease_0.reset();
  lease_1.reset();  // MRU

  std::unique_ptr<WebGpuSharedImageLease> lease_2 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_2->GetSharedImage().get());

  // LeaseSharedImage should return the MRU shared image, which
  // is that of lease_1, for lease_2.
  EXPECT_EQ(returned_shared_images[1], returned_shared_images[2]);
}

TEST_F(WebGpuSharedImageCacheTest, DifferentSize) {
  auto size1 = gfx::Size(10, 10);
  auto size2 = gfx::Size(20, 20);

  Vector<gpu::ClientSharedImage*> returned_shared_images;

  std::unique_ptr<WebGpuSharedImageLease> lease_0 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_0->GetSharedImage().get());

  std::unique_ptr<WebGpuSharedImageLease> lease_1 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size2, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_1->GetSharedImage().get());

  // Now release the leases to recycle the shared images.
  lease_1.reset();
  lease_0.reset();

  std::unique_ptr<WebGpuSharedImageLease> lease_2 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_2->GetSharedImage().get());

  std::unique_ptr<WebGpuSharedImageLease> lease_3 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size2, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_3->GetSharedImage().get());

  // LeaseSharedImage should return the same shared image
  // for the request with the same size.
  EXPECT_EQ(returned_shared_images[0], returned_shared_images[2]);
  EXPECT_EQ(returned_shared_images[1], returned_shared_images[3]);
}

TEST_F(WebGpuSharedImageCacheTest, CacheMissHit) {
  auto size1 = gfx::Size(10, 10);
  auto size2 = gfx::Size(20, 20);

  Vector<gpu::ClientSharedImage*> returned_shared_images;

  std::unique_ptr<WebGpuSharedImageLease> lease_0 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_0->GetSharedImage().get());

  // Now release the lease to recycle the shared image.
  lease_0.reset();

  // (1) For different size.
  std::unique_ptr<WebGpuSharedImageLease> lease_1 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size2, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_1->GetSharedImage().get());

  // Cache miss. A new shared image should be created.
  EXPECT_NE(returned_shared_images[0], returned_shared_images[1]);

  // (2) For different color space
  std::unique_ptr<WebGpuSharedImageLease> lease_2 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1,
      gfx::ColorSpace::CreateSRGBLinear(), kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_2->GetSharedImage().get());

  // Cache miss. A new shared image should be created.
  EXPECT_NE(returned_shared_images[0], returned_shared_images[2]);

  // (3) For different format
  std::unique_ptr<WebGpuSharedImageLease> lease_3 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_F16, size1, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_3->GetSharedImage().get());

  // Cache miss. A new shared image should be created.
  EXPECT_NE(returned_shared_images[0], returned_shared_images[3]);

  // (4) For different alpha type.
  std::unique_ptr<WebGpuSharedImageLease> lease_4 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1, gfx::ColorSpace::CreateSRGB(),
      kOpaque_SkAlphaType);
  returned_shared_images.push_back(lease_4->GetSharedImage().get());

  // Cache miss. A new shared image should be created.
  EXPECT_NE(returned_shared_images[0], returned_shared_images[4]);

  // (5) For the same config again.
  std::unique_ptr<WebGpuSharedImageLease> lease_5 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size1, gfx::ColorSpace::CreateSRGB(),
      kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_5->GetSharedImage().get());

  // Should get the same shared image.
  EXPECT_EQ(returned_shared_images[0], returned_shared_images[5]);
}

TEST_F(WebGpuSharedImageCacheTest, StaleResourcesCleanUp) {
  auto resource_size = gfx::Size(10, 10);
  Vector<gpu::ClientSharedImage*> returned_shared_images;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count = cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<WebGpuSharedImageLease> lease_0 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, resource_size,
      gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_0->GetSharedImage().get());

  std::unique_ptr<WebGpuSharedImageLease> lease_1 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, resource_size,
      gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_1->GetSharedImage().get());

  // Now release the leases to recycle the shared images.
  lease_0.reset();
  lease_1.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    wtf_size_t size = cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(2u, size);
  }

  // After the intended delay, all stale resources should be released now.
  wtf_size_t size_after = cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size_after);
}

TEST_F(WebGpuSharedImageCacheTest, ReuseBeforeCleanUp) {
  auto resource_size = gfx::Size(10, 10);
  Vector<gpu::ClientSharedImage*> returned_shared_images;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count = cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<WebGpuSharedImageLease> lease_0 = cache_->LeaseSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, resource_size,
      gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_shared_images.push_back(lease_0->GetSharedImage().get());

  // Release the lease to recycle the shared image.
  lease_0.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    if (i == 1) {
      // Now request a resource with the same configuration.
      std::unique_ptr<WebGpuSharedImageLease> lease_1 =
          cache_->LeaseSharedImage(viz::SinglePlaneFormat::kRGBA_8888,
                                   resource_size, gfx::ColorSpace::CreateSRGB(),
                                   kPremul_SkAlphaType);
      returned_shared_images.push_back(lease_1->GetSharedImage().get());

      // Release the leases again to recycle the shared images.
      lease_1.reset();
    }

    wtf_size_t size = cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(1u, size);
  }

  // Since the resource is reused before it gets deleted, it should not be
  // cleaned up on the next scheduled clean up. Instead, it will be cleaned up
  // with a new schedule.
  //
  wtf_size_t size = cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(1u, size);

  // Now, the resource should be deleted.
  size = cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size);
}


}  // namespace blink
