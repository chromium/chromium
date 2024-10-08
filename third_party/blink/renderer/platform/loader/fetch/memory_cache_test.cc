/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/testing/mock_context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class FakeDecodedResource final : public Resource {
 public:
  static FakeDecodedResource* Fetch(FetchParameters& params,
                                    ResourceFetcher* fetcher,
                                    ResourceClient* client) {
    return static_cast<FakeDecodedResource*>(
        fetcher->RequestResource(params, Factory(), client));
  }

  FakeDecodedResource(const ResourceRequest& request,
                      const ResourceLoaderOptions& options)
      : Resource(request, ResourceType::kMock, options) {}

  void AppendData(
      absl::variant<SegmentedBuffer, base::span<const char>> data) override {
    Resource::AppendData(std::move(data));
    SetDecodedSize(this->size());
  }

  void FakeEncodedSize(size_t size) { SetEncodedSize(size); }

 private:
  class Factory final : public NonTextResourceFactory {
   public:
    Factory() : NonTextResourceFactory(ResourceType::kMock) {}

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options) const override {
      return MakeGarbageCollected<FakeDecodedResource>(request, options);
    }
  };

  void DestroyDecodedDataIfPossible() override { SetDecodedSize(0); }
};

class MemoryCacheTest : public testing::Test {
 public:
  class FakeResource final : public Resource {
   public:
    static constexpr size_t kInitialDecodedSize = 42;

    FakeResource(const char* url, ResourceType type)
        : FakeResource(KURL(url), type) {}
    FakeResource(const KURL& url, ResourceType type)
        : FakeResource(ResourceRequest(url),
                       type,
                       ResourceLoaderOptions(nullptr /* world */)) {}
    FakeResource(const ResourceRequest& request,
                 ResourceType type,
                 const ResourceLoaderOptions& options)
        : Resource(request, type, options) {
      SetDecodedSize(kInitialDecodedSize);
    }

    void DestroyDecodedDataIfPossible() override { SetDecodedSize(0u); }
  };

 protected:
  void SetUp() override {
    // Save the global memory cache to restore it upon teardown.
    global_memory_cache_ = ReplaceMemoryCacheForTesting(
        MakeGarbageCollected<MemoryCache>(platform_->test_task_runner()));
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    lifecycle_notifier_ = MakeGarbageCollected<MockContextLifecycleNotifier>();
    fetcher_ = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), MakeGarbageCollected<MockFetchContext>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>(), lifecycle_notifier_,
        nullptr /* back_forward_cache_loader_helper */));
  }

  void TearDown() override {
    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  }

  Persistent<MemoryCache> global_memory_cache_;
  Persistent<ResourceFetcher> fetcher_;
  Persistent<MockContextLifecycleNotifier> lifecycle_notifier_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  base::test::TaskEnvironment task_environment_;
};


TEST_F(MemoryCacheTest, VeryLargeResourceAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kResourceSize1 = kSizeMax / 16;
  const size_t kResourceSize2 = kSizeMax / 20;
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  // Here and below, use an image MIME type. This is because on Android
  // non-image MIME types trigger a query to Java to check which video codecs
  // are supported. This fails in tests. The solution is either to use an image
  // type, or disable the tests on Android.
  // crbug.com/850788.
  FetchParameters params =
      FetchParameters::CreateForTest(ResourceRequest("data:image/jpeg,"));
  FakeDecodedResource* cached_resource =
      FakeDecodedResource::Fetch(params, fetcher_, client);
  cached_resource->FakeEncodedSize(kResourceSize1);

  EXPECT_TRUE(MemoryCache::Get()->Contains(cached_resource));
  EXPECT_EQ(cached_resource->size(), MemoryCache::Get()->size());

  client->RemoveAsClient();
  EXPECT_EQ(cached_resource->size(), MemoryCache::Get()->size());

  cached_resource->FakeEncodedSize(kResourceSize2);
  EXPECT_EQ(cached_resource->size(), MemoryCache::Get()->size());
}

// Verifies that
// - size() is updated appropriately when Resources are added to MemoryCache
//   and garbage collected.
// -
static void TestClientRemoval(ResourceFetcher* fetcher,
                              const String& identifier1,
                              const String& identifier2) {
  const std::string_view kData = "abcde";
  Persistent<MockResourceClient> client1 =
      MakeGarbageCollected<MockResourceClient>();
  Persistent<MockResourceClient> client2 =
      MakeGarbageCollected<MockResourceClient>();
  FetchParameters params1 =
      FetchParameters::CreateForTest(ResourceRequest("data:image/jpeg,foo"));
  Resource* resource1 = FakeDecodedResource::Fetch(params1, fetcher, client1);
  FetchParameters params2 =
      FetchParameters::CreateForTest(ResourceRequest("data:image/jpeg,bar"));
  Resource* resource2 = FakeDecodedResource::Fetch(params2, fetcher, client2);
  resource1->AppendData(kData.substr(0u, 4u));
  resource2->AppendData(kData.substr(0u, 4u));

  // Remove and re-Add the resources, with proper cache identifiers.
  MemoryCache::Get()->Remove(resource1);
  MemoryCache::Get()->Remove(resource2);
  if (!identifier1.empty())
    resource1->SetCacheIdentifier(identifier1);
  if (!identifier2.empty())
    resource2->SetCacheIdentifier(identifier2);
  MemoryCache::Get()->Add(resource1);
  MemoryCache::Get()->Add(resource2);

  size_t original_total_size = resource1->size() + resource2->size();

  // Removing the client from resource1 should not affect the size.
  client1->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, MemoryCache::Get()->size());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  // Removing the client from resource2 should not affect the size.
  client2->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, MemoryCache::Get()->size());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  WeakPersistent<Resource> resource1_weak = resource1;
  WeakPersistent<Resource> resource2_weak = resource2;

  // Garabage collection should cause resources without clients to be collected
  // and removed from the cache. The size should be updated accordingly.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(resource1_weak);
  EXPECT_FALSE(resource2_weak);
  EXPECT_EQ(0u, MemoryCache::Get()->size());
}

TEST_F(MemoryCacheTest, ClientRemoval_Basic) {
  TestClientRemoval(fetcher_, "", "");
}

TEST_F(MemoryCacheTest, ClientRemoval_MultipleResourceMaps) {
  {
    TestClientRemoval(fetcher_, "foo", "");
    MemoryCache::Get()->EvictResources();
  }
  {
    TestClientRemoval(fetcher_, "", "foo");
    MemoryCache::Get()->EvictResources();
  }
  {
    TestClientRemoval(fetcher_, "foo", "bar");
    MemoryCache::Get()->EvictResources();
  }
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation) {
  auto* resource1 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  MemoryCache::Get()->Add(resource1);

  auto* resource2 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  MemoryCache::Get()->Remove(resource1);
  MemoryCache::Get()->Add(resource2);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource1));

  auto* resource3 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  MemoryCache::Get()->Remove(resource2);
  MemoryCache::Get()->Add(resource3);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource3));
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource2));
}

TEST_F(MemoryCacheTest, ResourceMapIsolation) {
  auto* resource1 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  MemoryCache::Get()->Add(resource1);

  auto* resource2 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  resource2->SetCacheIdentifier("foo");
  MemoryCache::Get()->Add(resource2);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  const KURL url = KURL("http://test/resource");
  EXPECT_EQ(resource1, MemoryCache::Get()->ResourceForURLForTesting(url));
  EXPECT_EQ(resource1, MemoryCache::Get()->ResourceForURL(
                           url, MemoryCache::Get()->DefaultCacheIdentifier()));
  EXPECT_EQ(resource2, MemoryCache::Get()->ResourceForURL(url, "foo"));
  EXPECT_EQ(nullptr, MemoryCache::Get()->ResourceForURLForTesting(NullURL()));

  auto* resource3 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  resource3->SetCacheIdentifier("foo");
  MemoryCache::Get()->Remove(resource2);
  MemoryCache::Get()->Add(resource3);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource2));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource3));

  HeapVector<Member<Resource>> resources =
      MemoryCache::Get()->ResourcesForURL(url);
  EXPECT_EQ(2u, resources.size());

  MemoryCache::Get()->EvictResources();
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource1));
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource3));
}

TEST_F(MemoryCacheTest, FragmentIdentifier) {
  const KURL url1 = KURL("http://test/resource#foo");
  auto* resource = MakeGarbageCollected<FakeResource>(url1, ResourceType::kRaw);
  MemoryCache::Get()->Add(resource);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource));

  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url1));

  const KURL url2 = MemoryCache::RemoveFragmentIdentifierIfNeeded(url1);
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURLForTesting(url2));
}

TEST_F(MemoryCacheTest, RemoveURLFromCache) {
  const KURL url1 = KURL("http://test/resource1");
  Persistent<FakeResource> resource1 =
      MakeGarbageCollected<FakeResource>(url1, ResourceType::kRaw);
  MemoryCache::Get()->Add(resource1);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));

  MemoryCache::Get()->RemoveURLFromCache(url1);
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource1));

  const KURL url2 = KURL("http://test/resource2#foo");
  auto* resource2 =
      MakeGarbageCollected<FakeResource>(url2, ResourceType::kRaw);
  MemoryCache::Get()->Add(resource2);
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  MemoryCache::Get()->RemoveURLFromCache(url2);
  EXPECT_FALSE(MemoryCache::Get()->Contains(resource2));
}

class MemoryCacheStrongReferenceTest : public MemoryCacheTest {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features = {
      features::kMemoryCacheStrongReference
    };
    scoped_feature_list_.InitWithFeatures(enable_features, {});
    MemoryCacheTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MemoryCacheStrongReferenceTest, ResourceTimeout) {
  const KURL url = KURL("http://test/resource1");
  Member<FakeResource> resource =
      MakeGarbageCollected<FakeResource>(url, ResourceType::kRaw);

  ASSERT_EQ(MemoryCache::Get()->strong_references_.size(), 0u);
  MemoryCache::Get()->strong_references_prune_duration_ = base::Milliseconds(1);
  MemoryCache::Get()->SavePageResourceStrongReferences(
      HeapVector<Member<Resource>>{resource});
  ASSERT_EQ(MemoryCache::Get()->strong_references_.size(), 1u);

  (*MemoryCache::Get()->strong_references_.begin())
      ->memory_cache_last_accessed_ = base::TimeTicks();
  platform_->test_task_runner()->FastForwardBy(base::Minutes(5) +
                                               base::Seconds(1));
  ASSERT_EQ(MemoryCache::Get()->strong_references_.size(), 0u);
}

TEST_F(MemoryCacheStrongReferenceTest, LRU) {
  const KURL url1 = KURL("http://test/resource1");
  const KURL url2 = KURL("http://test/resource1");
  Member<FakeResource> resource1 =
      MakeGarbageCollected<FakeResource>(url1, ResourceType::kRaw);
  Member<FakeResource> resource2 =
      MakeGarbageCollected<FakeResource>(url2, ResourceType::kRaw);
  MemoryCache::Get()->SaveStrongReference(resource1);
  MemoryCache::Get()->SaveStrongReference(resource2);
  MemoryCache::Get()->SaveStrongReference(resource1);
  ASSERT_EQ(MemoryCache::Get()->strong_references_.size(), 2u);
  ASSERT_EQ(*MemoryCache::Get()->strong_references_.begin(), resource2.Get());
}

TEST_F(MemoryCacheStrongReferenceTest, ClearStrongReferences) {
  const KURL kURL("http://test/resource1");
  Member<FakeResource> resource =
      MakeGarbageCollected<FakeResource>(kURL, ResourceType::kRaw);
  MemoryCache::Get()->SaveStrongReference(resource);
  EXPECT_EQ(MemoryCache::Get()->strong_references_.size(), 1u);
  MemoryCache::Get()->ClearStrongReferences();
  EXPECT_EQ(MemoryCache::Get()->strong_references_.size(), 0u);
}

}  // namespace blink
