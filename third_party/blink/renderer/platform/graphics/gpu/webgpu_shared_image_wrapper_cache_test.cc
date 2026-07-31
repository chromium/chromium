// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper_cache.h"

#include <array>

#include "base/test/task_environment.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

namespace blink {

class WebGpuSharedImageWrapperCacheTest : public testing::Test {
 public:
  WebGpuSharedImageWrapperCacheTest() = default;
  ~WebGpuSharedImageWrapperCacheTest() override = default;

  // Implements testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<WebGpuSharedImageWrapperCache> wrapper_cache_;
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

void WebGpuSharedImageWrapperCacheTest::SetUp() {
  Platform::SetMainThreadTaskRunnerForTesting();
  test_context_provider_ = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContext(test_context_provider_.get(),
                             &image_decode_cache_);

  wrapper_cache_ = std::make_unique<WebGpuSharedImageWrapperCache>(
      SharedGpuContext::ContextProviderWrapper(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebGpuSharedImageWrapperCacheTest::TearDown() {
  Platform::UnsetMainThreadTaskRunnerForTesting();
  SharedGpuContext::Reset();
}

TEST_F(WebGpuSharedImageWrapperCacheTest, MRUSameSize) {
  auto size = gfx::Size(10, 10);
  Vector<WebGpuSharedImageWrapper*> returned_wrappers;

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_0 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_0->shared_image_wrapper());

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_1 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_1->shared_image_wrapper());

  // Now release the leases to recycle the wrappers.
  wrapper_lease_0.reset();
  wrapper_lease_1.reset();  // MRU

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_2 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_2->shared_image_wrapper());

  // LeaseWebGpuSharedImageWrapper should return the MRU wrapper, which
  // is wrapper_lease_1, for wrapper_lease_2.
  EXPECT_EQ(returned_wrappers[1], returned_wrappers[2]);
}

TEST_F(WebGpuSharedImageWrapperCacheTest, DifferentSize) {
  auto size1 = gfx::Size(10, 10);
  auto size2 = gfx::Size(20, 20);

  Vector<WebGpuSharedImageWrapper*> returned_wrappers;

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_0 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_0->shared_image_wrapper());

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_1 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size2,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_1->shared_image_wrapper());

  // Now release the leases to recycle the wrappers.
  wrapper_lease_1.reset();
  wrapper_lease_0.reset();

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_2 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_2->shared_image_wrapper());

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_3 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size2,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_3->shared_image_wrapper());

  // LeaseWebGpuSharedImageWrapper should return the same shared image
  // wrapper for the request with the same size.
  EXPECT_EQ(returned_wrappers[0], returned_wrappers[2]);
  EXPECT_EQ(returned_wrappers[1], returned_wrappers[3]);
}

TEST_F(WebGpuSharedImageWrapperCacheTest, CacheMissHit) {
  auto size1 = gfx::Size(10, 10);
  auto size2 = gfx::Size(20, 20);

  Vector<WebGpuSharedImageWrapper*> returned_wrappers;

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_0 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_0->shared_image_wrapper());

  // Now release the lease to recycle the wrapper.
  wrapper_lease_0.reset();

  // (1) For different size.
  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_1 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size2,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_1->shared_image_wrapper());

  // Cache miss. A new wrapper should be created.
  EXPECT_NE(returned_wrappers[0], returned_wrappers[1]);

  // (2) For different color space
  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_2 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGBLinear(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_2->shared_image_wrapper());

  // Cache miss. A new wrapper should be created.
  EXPECT_NE(returned_wrappers[0], returned_wrappers[2]);

  // (3) For different format
  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_3 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_F16, size1,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_3->shared_image_wrapper());

  // Cache miss. A new wrapper should be created.
  EXPECT_NE(returned_wrappers[0], returned_wrappers[3]);

  // (4) For different alpha type.
  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_4 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGB(), kOpaque_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_4->shared_image_wrapper());

  // Cache miss. A new wrapper should be created.
  EXPECT_NE(returned_wrappers[0], returned_wrappers[4]);

  // (5) For the same config again.
  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_5 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, size1,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_5->shared_image_wrapper());

  // Should get the same wrapper.
  EXPECT_EQ(returned_wrappers[0], returned_wrappers[5]);
}

TEST_F(WebGpuSharedImageWrapperCacheTest, StaleResourcesCleanUp) {
  auto resource_size = gfx::Size(10, 10);
  Vector<WebGpuSharedImageWrapper*> returned_wrappers;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count = wrapper_cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_0 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, resource_size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_0->shared_image_wrapper());

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_1 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, resource_size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_1->shared_image_wrapper());

  // Now release the leases to recycle the wrappers.
  wrapper_lease_0.reset();
  wrapper_lease_1.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    wtf_size_t size = wrapper_cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(2u, size);
  }

  // After the intended delay, all stale resources should be released now.
  wtf_size_t size_after =
      wrapper_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size_after);
}

TEST_F(WebGpuSharedImageWrapperCacheTest, ReuseBeforeCleanUp) {
  auto resource_size = gfx::Size(10, 10);
  Vector<WebGpuSharedImageWrapper*> returned_wrappers;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count = wrapper_cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_0 =
      wrapper_cache_->LeaseWebGpuSharedImageWrapper(
          viz::SinglePlaneFormat::kRGBA_8888, resource_size,
          gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
  returned_wrappers.push_back(wrapper_lease_0->shared_image_wrapper());

  // Release the lease to recycle the wrapper.
  wrapper_lease_0.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    if (i == 1) {
      // Now request a resource with the same configuration.
      std::unique_ptr<WebGpuSharedImageWrapperLease> wrapper_lease_1 =
          wrapper_cache_->LeaseWebGpuSharedImageWrapper(
              viz::SinglePlaneFormat::kRGBA_8888, resource_size,
              gfx::ColorSpace::CreateSRGB(), kPremul_SkAlphaType);
      returned_wrappers.push_back(wrapper_lease_1->shared_image_wrapper());

      // Release the leases again to recycle the wrappers.
      wrapper_lease_1.reset();
    }

    wtf_size_t size = wrapper_cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(1u, size);
  }

  // Since the resource is reused before it gets deleted, it should not be
  // cleaned up on the next scheduled clean up. Instead, it will be cleaned up
  // with a new schedule.
  //
  wtf_size_t size = wrapper_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(1u, size);

  // Now, the resource should be deleted.
  size = wrapper_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size);
}

}  // namespace blink
