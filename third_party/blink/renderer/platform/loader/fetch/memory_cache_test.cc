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

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  void AppendData(const char* data, size_t len) override {
    Resource::AppendData(data, len);
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
    FakeResource(const char* url, ResourceType type)
        : FakeResource(KURL(url), type) {}
    FakeResource(const KURL& url, ResourceType type)
        : FakeResource(ResourceRequest(url),
                       type,
                       ResourceLoaderOptions(nullptr /* world */)) {}
    FakeResource(const ResourceRequest& request,
                 ResourceType type,
                 const ResourceLoaderOptions& options)
        : Resource(request, type, options) {}
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

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  MemoryCache::Get()->SetCapacity(kTotalCapacity);
  EXPECT_EQ(kTotalCapacity, MemoryCache::Get()->Capacity());
}

TEST_F(MemoryCacheTest, VeryLargeResourceAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  const size_t kResourceSize1 = kSizeMax / 16;
  const size_t kResourceSize2 = kSizeMax / 20;
  MemoryCache::Get()->SetCapacity(kTotalCapacity);
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

static void RunTask(Resource* resource1, Resource* resource2) {
  // The resource size has to be nonzero for this test to be meaningful, but
  // we do not rely on it having any particular value.
  EXPECT_GT(resource1->size(), 0u);
  EXPECT_GT(resource2->size(), 0u);

  EXPECT_EQ(0u, MemoryCache::Get()->size());

  MemoryCache::Get()->Add(resource1);
  MemoryCache::Get()->Add(resource2);

  size_t total_size = resource1->size() + resource2->size();
  EXPECT_EQ(total_size, MemoryCache::Get()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);

  // We expect actual pruning doesn't occur here synchronously but deferred,
  // due to the previous pruning invoked in TestResourcePruningLater().
  MemoryCache::Get()->Prune();
  EXPECT_EQ(total_size, MemoryCache::Get()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
}

static void TestResourcePruningLater(ResourceFetcher* fetcher,
                                     const String& identifier1,
                                     const String& identifier2) {
  auto* platform = static_cast<TestingPlatformSupportWithMockScheduler*>(
      Platform::Current());

  MemoryCache::Get()->SetDelayBeforeLiveDecodedPrune(base::TimeDelta());

  // Enforce pruning by adding |dummyResource| and then call prune().
  Resource* dummy_resource = RawResource::CreateForTest(
      KURL("http://dummy"), SecurityOrigin::CreateUniqueOpaque(),
      ResourceType::kRaw);
  MemoryCache::Get()->Add(dummy_resource);
  EXPECT_GT(MemoryCache::Get()->size(), 1u);
  const unsigned kTotalCapacity = 1;
  MemoryCache::Get()->SetCapacity(kTotalCapacity);
  MemoryCache::Get()->Prune();
  MemoryCache::Get()->Remove(dummy_resource);
  EXPECT_EQ(0u, MemoryCache::Get()->size());

  const char kData[6] = "abcde";
  FetchParameters params1 = FetchParameters::CreateForTest(
      ResourceRequest("data:image/jpeg,resource1"));
  Resource* resource1 = FakeDecodedResource::Fetch(params1, fetcher, nullptr);
  MemoryCache::Get()->Remove(resource1);
  if (!identifier1.empty())
    resource1->SetCacheIdentifier(identifier1);
  resource1->AppendData(kData, 3u);
  resource1->FinishForTest();
  FetchParameters params2 = FetchParameters::CreateForTest(
      ResourceRequest("data:image/jpeg,resource2"));
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  Resource* resource2 = FakeDecodedResource::Fetch(params2, fetcher, client);
  MemoryCache::Get()->Remove(resource2);
  if (!identifier2.empty())
    resource2->SetCacheIdentifier(identifier2);
  resource2->AppendData(kData, 4u);
  resource2->FinishForTest();

  platform->test_task_runner()->PostTask(
      FROM_HERE, WTF::BindOnce(&RunTask, WrapPersistent(resource1),
                               WrapPersistent(resource2)));
  platform->RunUntilIdle();

  // Now, the resources was pruned.
  size_t size_without_decode =
      resource1->EncodedSize() + resource1->OverheadSize() +
      resource2->EncodedSize() + resource2->OverheadSize();
  EXPECT_EQ(size_without_decode, MemoryCache::Get()->size());
}

// Verified that when ordering a prune in a runLoop task, the prune is deferred.
TEST_F(MemoryCacheTest, ResourcePruningLater_Basic) {
  TestResourcePruningLater(fetcher_, "", "");
}

TEST_F(MemoryCacheTest, ResourcePruningLater_MultipleResourceMaps) {
  {
    TestResourcePruningLater(fetcher_, "foo", "");
    MemoryCache::Get()->EvictResources();
  }
  {
    TestResourcePruningLater(fetcher_, "foo", "bar");
    MemoryCache::Get()->EvictResources();
  }
}

// Verifies that
// - Resources are not pruned synchronously when ResourceClient is removed.
// - size() is updated appropriately when Resources are added to MemoryCache
//   and garbage collected.
static void TestClientRemoval(ResourceFetcher* fetcher,
                              const String& identifier1,
                              const String& identifier2) {
  MemoryCache::Get()->SetCapacity(0);
  const char kData[6] = "abcde";
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
  resource1->AppendData(kData, 4u);
  resource2->AppendData(kData, 4u);

  MemoryCache::Get()->SetCapacity(0);
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

  // Call prune. There is nothing to prune, but this will initialize
  // the prune timestamp, allowing future prunes to be deferred.
  MemoryCache::Get()->Prune();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, MemoryCache::Get()->size());

  // Removing the client from resource1 should not trigger pruning.
  client1->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, MemoryCache::Get()->size());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  // Removing the client from resource2 should not trigger pruning.
  client2->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, MemoryCache::Get()->size());
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource1));
  EXPECT_TRUE(MemoryCache::Get()->Contains(resource2));

  WeakPersistent<Resource> resource1_weak = resource1;
  WeakPersistent<Resource> resource2_weak = resource2;

  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  // Resources are garbage-collected (WeakMemoryCache) and thus removed
  // from MemoryCache.
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
  EXPECT_EQ(resource1, MemoryCache::Get()->ResourceForURL(url));
  EXPECT_EQ(resource1, MemoryCache::Get()->ResourceForURL(
                           url, MemoryCache::Get()->DefaultCacheIdentifier()));
  EXPECT_EQ(resource2, MemoryCache::Get()->ResourceForURL(url, "foo"));
  EXPECT_EQ(nullptr, MemoryCache::Get()->ResourceForURL(NullURL()));

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

  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURL(url1));

  const KURL url2 = MemoryCache::RemoveFragmentIdentifierIfNeeded(url1);
  EXPECT_EQ(resource, MemoryCache::Get()->ResourceForURL(url2));
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

}  // namespace blink
