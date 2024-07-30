// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

#include "base/test/task_environment.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

namespace blink {

class WebGPURecyclableResourceCacheTest : public testing::Test {
 public:
  WebGPURecyclableResourceCacheTest() = default;
  ~WebGPURecyclableResourceCacheTest() override = default;

  // Implements testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<WebGPURecyclableResourceCache> recyclable_resource_cache_;
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

void WebGPURecyclableResourceCacheTest::SetUp() {
  Platform::SetMainThreadTaskRunnerForTesting();
  test_context_provider_ = viz::TestContextProvider::Create();
  InitializeSharedGpuContextGLES2(test_context_provider_.get(),
                                  &image_decode_cache_);

  recyclable_resource_cache_ = std::make_unique<WebGPURecyclableResourceCache>(
      SharedGpuContext::ContextProviderWrapper(),
      scheduler::GetSingleThreadTaskRunnerForTesting());
}

void WebGPURecyclableResourceCacheTest::TearDown() {
  Platform::UnsetMainThreadTaskRunnerForTesting();
  SharedGpuContext::Reset();
}

TEST_F(WebGPURecyclableResourceCacheTest, MRUSameSize) {
  SkImageInfo kInfo =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  Vector<CanvasResourceProvider*> returned_resource_providers;

  std::unique_ptr<RecyclableCanvasResource> provider_holder_0 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_0->resource_provider());

  std::unique_ptr<RecyclableCanvasResource> provider_holder_1 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_1->resource_provider());

  // Now release the holders to recycle the resource_providers.
  provider_holder_0.reset();
  provider_holder_1.reset();  // MRU

  std::unique_ptr<RecyclableCanvasResource> provider_holder_2 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_2->resource_provider());

  // GetOrCreateCanvasResource should return the MRU provider, which is
  // provider_holder_1, for provider_holder_2.
  EXPECT_EQ(returned_resource_providers[1], returned_resource_providers[2]);
}

TEST_F(WebGPURecyclableResourceCacheTest, DifferentSize) {
  const SkImageInfo kInfos[] = {
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      SkImageInfo::Make(20, 20, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
  };
  Vector<CanvasResourceProvider*> returned_resource_providers;

  std::unique_ptr<RecyclableCanvasResource> provider_holder_0 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfos[0]);
  returned_resource_providers.push_back(provider_holder_0->resource_provider());

  std::unique_ptr<RecyclableCanvasResource> provider_holder_1 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfos[1]);
  returned_resource_providers.push_back(provider_holder_1->resource_provider());

  // Now release the holders to recycle the resource_providers.
  provider_holder_1.reset();
  provider_holder_0.reset();

  std::unique_ptr<RecyclableCanvasResource> provider_holder_2 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfos[0]);
  returned_resource_providers.push_back(provider_holder_2->resource_provider());

  std::unique_ptr<RecyclableCanvasResource> provider_holder_3 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfos[1]);
  returned_resource_providers.push_back(provider_holder_3->resource_provider());

  // GetOrCreateCanvasResource should return the same resource provider
  // for the request with the same size.
  EXPECT_EQ(returned_resource_providers[0], returned_resource_providers[2]);
  EXPECT_EQ(returned_resource_providers[1], returned_resource_providers[3]);
}

TEST_F(WebGPURecyclableResourceCacheTest, CacheMissHit) {
  Vector<CanvasResourceProvider*> returned_resource_providers;

  const auto info_0 =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  std::unique_ptr<RecyclableCanvasResource> provider_holder_0 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_0);
  returned_resource_providers.push_back(provider_holder_0->resource_provider());

  // Now release the holder to recycle the resource_provider.
  provider_holder_0.reset();

  // (1) For different size.
  const auto info_1 =
      SkImageInfo::Make(20, 20, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  std::unique_ptr<RecyclableCanvasResource> provider_holder_1 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_1);
  returned_resource_providers.push_back(provider_holder_1->resource_provider());

  // Cache miss. A new resource provider should be created.
  EXPECT_NE(returned_resource_providers[0], returned_resource_providers[1]);

  // (2) For different SkImageInfo: color space
  const SkImageInfo info_2 =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                        SkColorSpace::MakeSRGBLinear());
  std::unique_ptr<RecyclableCanvasResource> provider_holder_2 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_2);
  returned_resource_providers.push_back(provider_holder_2->resource_provider());

  // Cache miss. A new resource provider should be created.
  EXPECT_NE(returned_resource_providers[0], returned_resource_providers[2]);

  // (3) For different SkImageInfo: color type.
  const SkImageInfo info_3 =
      SkImageInfo::Make(10, 10, kRGBA_F16_SkColorType, kPremul_SkAlphaType);
  std::unique_ptr<RecyclableCanvasResource> provider_holder_3 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_3);
  returned_resource_providers.push_back(provider_holder_3->resource_provider());

  // Cache miss. A new resource provider should be created.
  EXPECT_NE(returned_resource_providers[0], returned_resource_providers[3]);

  // (4) For different SkImageInfo: alpha type.
  const auto info_4 =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
  std::unique_ptr<RecyclableCanvasResource> provider_holder_4 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_4);
  returned_resource_providers.push_back(provider_holder_4->resource_provider());

  // Cache miss. A new resource provider should be created.
  EXPECT_NE(returned_resource_providers[0], returned_resource_providers[4]);

  // (5) For the same config again.
  std::unique_ptr<RecyclableCanvasResource> provider_holder_5 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(info_0);
  returned_resource_providers.push_back(provider_holder_5->resource_provider());

  // Should get the same provider.
  EXPECT_EQ(returned_resource_providers[0], returned_resource_providers[5]);
}

TEST_F(WebGPURecyclableResourceCacheTest, StaleResourcesCleanUp) {
  SkImageInfo kInfo =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType);

  Vector<CanvasResourceProvider*> returned_resource_providers;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count =
      recyclable_resource_cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<RecyclableCanvasResource> provider_holder_0 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_0->resource_provider());

  std::unique_ptr<RecyclableCanvasResource> provider_holder_1 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_1->resource_provider());

  // Now release the holders to recycle the resource_providers.
  provider_holder_0.reset();
  provider_holder_1.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    wtf_size_t size =
        recyclable_resource_cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(2u, size);
  }

  // After the intended delay, all stale resources should be released now.
  wtf_size_t size_after =
      recyclable_resource_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size_after);
}

TEST_F(WebGPURecyclableResourceCacheTest, ReuseBeforeCleanUp) {
  SkImageInfo kInfo =
      SkImageInfo::Make(10, 10, kRGBA_8888_SkColorType, kPremul_SkAlphaType);

  Vector<CanvasResourceProvider*> returned_resource_providers;
  // The loop count for CleanUpResources before the resource gets cleaned up.
  int wait_count =
      recyclable_resource_cache_->GetWaitCountBeforeDeletionForTesting();

  std::unique_ptr<RecyclableCanvasResource> provider_holder_0 =
      recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
  returned_resource_providers.push_back(provider_holder_0->resource_provider());

  // Release the holder to recycle the resource_provider.
  provider_holder_0.reset();

  // Before the intended delay, the recycled resources should not be released
  // from cache.
  for (int i = 0; i < wait_count; i++) {
    if (i == 1) {
      // Now request a resource with the same configuration.
      std::unique_ptr<RecyclableCanvasResource> provider_holder_1 =
          recyclable_resource_cache_->GetOrCreateCanvasResource(kInfo);
      returned_resource_providers.push_back(
          provider_holder_1->resource_provider());

      // Release the holders again to recycle the resource_providers.
      provider_holder_1.reset();
    }

    wtf_size_t size =
        recyclable_resource_cache_->CleanUpResourcesAndReturnSizeForTesting();
    EXPECT_EQ(1u, size);
  }

  // Since the resource is reused before it gets deleted, it should not be
  // cleaned up on the next scheduled clean up. Instead, it will be cleaned up
  // with a new schedule.
  //
  wtf_size_t size =
      recyclable_resource_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(1u, size);

  // Now, the resource should be deleted.
  size = recyclable_resource_cache_->CleanUpResourcesAndReturnSizeForTesting();
  EXPECT_EQ(0u, size);
}

}  // namespace blink
