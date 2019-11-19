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

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource_client.h"
#include "third_party/blink/renderer/platform/loader/testing/test_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
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
        : FakeResource(ResourceRequest(url), type, ResourceLoaderOptions()) {}
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
    fetcher_ = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties->MakeDetachable(), MakeGarbageCollected<MockFetchContext>(),
        base::MakeRefCounted<scheduler::FakeTaskRunner>(),
        MakeGarbageCollected<TestLoaderFactory>()));
  }

  void TearDown() override {
    ReplaceMemoryCacheForTesting(global_memory_cache_.Release());
  }

  Persistent<MemoryCache> global_memory_cache_;
  Persistent<ResourceFetcher> fetcher_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

// Verifies that setters and getters for cache capacities work correcty.
TEST_F(MemoryCacheTest, CapacityAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  EXPECT_EQ(kTotalCapacity, GetMemoryCache()->Capacity());
}

// TODO(crbug.com/850788): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_VeryLargeResourceAccounting DISABLED_VeryLargeResourceAccounting
#else
#define MAYBE_VeryLargeResourceAccounting VeryLargeResourceAccounting
#endif
TEST_F(MemoryCacheTest, MAYBE_VeryLargeResourceAccounting) {
  const size_t kSizeMax = ~static_cast<size_t>(0);
  const size_t kTotalCapacity = kSizeMax / 4;
  const size_t kResourceSize1 = kSizeMax / 16;
  const size_t kResourceSize2 = kSizeMax / 20;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  FetchParameters params(ResourceRequest("data:text/html,"));
  FakeDecodedResource* cached_resource =
      FakeDecodedResource::Fetch(params, fetcher_, client);
  cached_resource->FakeEncodedSize(kResourceSize1);

  EXPECT_TRUE(GetMemoryCache()->Contains(cached_resource));
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());

  client->RemoveAsClient();
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());

  cached_resource->FakeEncodedSize(kResourceSize2);
  EXPECT_EQ(cached_resource->size(), GetMemoryCache()->size());
}

static void RunTask(Resource* resource1, Resource* resource2) {
  // The resource size has to be nonzero for this test to be meaningful, but
  // we do not rely on it having any particular value.
  EXPECT_GT(resource1->size(), 0u);
  EXPECT_GT(resource2->size(), 0u);

  EXPECT_EQ(0u, GetMemoryCache()->size());

  GetMemoryCache()->Add(resource1);
  GetMemoryCache()->Add(resource2);

  size_t total_size = resource1->size() + resource2->size();
  EXPECT_EQ(total_size, GetMemoryCache()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);

  // We expect actual pruning doesn't occur here synchronously but deferred,
  // due to the previous pruning invoked in TestResourcePruningLater().
  GetMemoryCache()->Prune();
  EXPECT_EQ(total_size, GetMemoryCache()->size());
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
}

static void TestResourcePruningLater(ResourceFetcher* fetcher,
                                     const String& identifier1,
                                     const String& identifier2) {
  auto* platform = static_cast<TestingPlatformSupportWithMockScheduler*>(
      Platform::Current());

  GetMemoryCache()->SetDelayBeforeLiveDecodedPrune(base::TimeDelta());

  // Enforce pruning by adding |dummyResource| and then call prune().
  Resource* dummy_resource = RawResource::CreateForTest(
      KURL("http://dummy"), SecurityOrigin::CreateUniqueOpaque(),
      ResourceType::kRaw);
  GetMemoryCache()->Add(dummy_resource);
  EXPECT_GT(GetMemoryCache()->size(), 1u);
  const unsigned kTotalCapacity = 1;
  GetMemoryCache()->SetCapacity(kTotalCapacity);
  GetMemoryCache()->Prune();
  GetMemoryCache()->Remove(dummy_resource);
  EXPECT_EQ(0u, GetMemoryCache()->size());

  const char kData[6] = "abcde";
  FetchParameters params1(ResourceRequest("data:text/html,resource1"));
  Resource* resource1 = FakeDecodedResource::Fetch(params1, fetcher, nullptr);
  GetMemoryCache()->Remove(resource1);
  if (!identifier1.IsEmpty())
    resource1->SetCacheIdentifier(identifier1);
  resource1->AppendData(kData, 3u);
  resource1->FinishForTest();
  FetchParameters params2(ResourceRequest("data:text/html,resource2"));
  Persistent<MockResourceClient> client =
      MakeGarbageCollected<MockResourceClient>();
  Resource* resource2 = FakeDecodedResource::Fetch(params2, fetcher, client);
  GetMemoryCache()->Remove(resource2);
  if (!identifier2.IsEmpty())
    resource2->SetCacheIdentifier(identifier2);
  resource2->AppendData(kData, 4u);
  resource2->FinishForTest();

  platform->test_task_runner()->PostTask(
      FROM_HERE, WTF::Bind(&RunTask, WrapPersistent(resource1),
                           WrapPersistent(resource2)));
  platform->RunUntilIdle();

  // Now, the resources was pruned.
  unsigned size_without_decode =
      resource1->EncodedSize() + resource1->OverheadSize() +
      resource2->EncodedSize() + resource2->OverheadSize();
  EXPECT_EQ(size_without_decode, GetMemoryCache()->size());
}

// Verified that when ordering a prune in a runLoop task, the prune is deferred.
// TODO(crbug.com/850788): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_ResourcePruningLater_Basic DISABLED_ResourcePruningLater_Basic
#else
#define MAYBE_ResourcePruningLater_Basic ResourcePruningLater_Basic
#endif
TEST_F(MemoryCacheTest, MAYBE_ResourcePruningLater_Basic) {
  TestResourcePruningLater(fetcher_, "", "");
}

// TODO(crbug.com/850788): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_ResourcePruningLater_MultipleResourceMaps \
  DISABLED_ResourcePruningLater_MultipleResourceMaps
#else
#define MAYBE_ResourcePruningLater_MultipleResourceMaps \
  ResourcePruningLater_MultipleResourceMaps
#endif
TEST_F(MemoryCacheTest, MAYBE_ResourcePruningLater_MultipleResourceMaps) {
  {
    TestResourcePruningLater(fetcher_, "foo", "");
    GetMemoryCache()->EvictResources();
  }
  {
    TestResourcePruningLater(fetcher_, "foo", "bar");
    GetMemoryCache()->EvictResources();
  }
}

// Verifies that
// - Resources are not pruned synchronously when ResourceClient is removed.
// - size() is updated appropriately when Resources are added to MemoryCache
//   and garbage collected.
static void TestClientRemoval(ResourceFetcher* fetcher,
                              const String& identifier1,
                              const String& identifier2) {
  GetMemoryCache()->SetCapacity(0);
  const char kData[6] = "abcde";
  Persistent<MockResourceClient> client1 =
      MakeGarbageCollected<MockResourceClient>();
  Persistent<MockResourceClient> client2 =
      MakeGarbageCollected<MockResourceClient>();
  FetchParameters params1(ResourceRequest("data:text/html,foo"));
  Resource* resource1 = FakeDecodedResource::Fetch(params1, fetcher, client1);
  FetchParameters params2(ResourceRequest("data:text/html,bar"));
  Resource* resource2 = FakeDecodedResource::Fetch(params2, fetcher, client2);
  resource1->AppendData(kData, 4u);
  resource2->AppendData(kData, 4u);

  GetMemoryCache()->SetCapacity(0);
  // Remove and re-Add the resources, with proper cache identifiers.
  GetMemoryCache()->Remove(resource1);
  GetMemoryCache()->Remove(resource2);
  if (!identifier1.IsEmpty())
    resource1->SetCacheIdentifier(identifier1);
  if (!identifier2.IsEmpty())
    resource2->SetCacheIdentifier(identifier2);
  GetMemoryCache()->Add(resource1);
  GetMemoryCache()->Add(resource2);

  size_t original_total_size = resource1->size() + resource2->size();

  // Call prune. There is nothing to prune, but this will initialize
  // the prune timestamp, allowing future prunes to be deferred.
  GetMemoryCache()->Prune();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());

  // Removing the client from resource1 should not trigger pruning.
  client1->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  // Removing the client from resource2 should not trigger pruning.
  client2->RemoveAsClient();
  EXPECT_GT(resource1->DecodedSize(), 0u);
  EXPECT_GT(resource2->DecodedSize(), 0u);
  EXPECT_EQ(original_total_size, GetMemoryCache()->size());
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  WeakPersistent<Resource> resource1_weak = resource1;
  WeakPersistent<Resource> resource2_weak = resource2;

  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);
  // Resources are garbage-collected (WeakMemoryCache) and thus removed
  // from MemoryCache.
  EXPECT_FALSE(resource1_weak);
  EXPECT_FALSE(resource2_weak);
  EXPECT_EQ(0u, GetMemoryCache()->size());
}

// TODO(crbug.com/850788): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_ClientRemoval_Basic DISABLED_ClientRemoval_Basic
#else
#define MAYBE_ClientRemoval_Basic ClientRemoval_Basic
#endif
TEST_F(MemoryCacheTest, MAYBE_ClientRemoval_Basic) {
  TestClientRemoval(fetcher_, "", "");
}

// TODO(crbug.com/850788): Reenable this.
#if defined(OS_ANDROID)
#define MAYBE_ClientRemoval_MultipleResourceMaps \
  DISABLED_ClientRemoval_MultipleResourceMaps
#else
#define MAYBE_ClientRemoval_MultipleResourceMaps \
  ClientRemoval_MultipleResourceMaps
#endif
TEST_F(MemoryCacheTest, MAYBE_ClientRemoval_MultipleResourceMaps) {
  {
    TestClientRemoval(fetcher_, "foo", "");
    GetMemoryCache()->EvictResources();
  }
  {
    TestClientRemoval(fetcher_, "", "foo");
    GetMemoryCache()->EvictResources();
  }
  {
    TestClientRemoval(fetcher_, "foo", "bar");
    GetMemoryCache()->EvictResources();
  }
}

TEST_F(MemoryCacheTest, RemoveDuringRevalidation) {
  auto* resource1 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  GetMemoryCache()->Add(resource1);

  auto* resource2 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  GetMemoryCache()->Remove(resource1);
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));

  auto* resource3 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  GetMemoryCache()->Remove(resource2);
  GetMemoryCache()->Add(resource3);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource3));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
}

TEST_F(MemoryCacheTest, ResourceMapIsolation) {
  auto* resource1 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  GetMemoryCache()->Add(resource1);

  auto* resource2 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  resource2->SetCacheIdentifier("foo");
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  const KURL url = KURL("http://test/resource");
  EXPECT_EQ(resource1, GetMemoryCache()->ResourceForURL(url));
  EXPECT_EQ(resource1, GetMemoryCache()->ResourceForURL(
                           url, GetMemoryCache()->DefaultCacheIdentifier()));
  EXPECT_EQ(resource2, GetMemoryCache()->ResourceForURL(url, "foo"));
  EXPECT_EQ(nullptr, GetMemoryCache()->ResourceForURL(NullURL()));

  auto* resource3 = MakeGarbageCollected<FakeResource>("http://test/resource",
                                                       ResourceType::kRaw);
  resource3->SetCacheIdentifier("foo");
  GetMemoryCache()->Remove(resource2);
  GetMemoryCache()->Add(resource3);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
  EXPECT_TRUE(GetMemoryCache()->Contains(resource3));

  HeapVector<Member<Resource>> resources =
      GetMemoryCache()->ResourcesForURL(url);
  EXPECT_EQ(2u, resources.size());

  GetMemoryCache()->EvictResources();
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource3));
}

TEST_F(MemoryCacheTest, FragmentIdentifier) {
  const KURL url1 = KURL("http://test/resource#foo");
  auto* resource = MakeGarbageCollected<FakeResource>(url1, ResourceType::kRaw);
  GetMemoryCache()->Add(resource);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource));

  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url1));

  const KURL url2 = MemoryCache::RemoveFragmentIdentifierIfNeeded(url1);
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url2));
}

TEST_F(MemoryCacheTest, RemoveURLFromCache) {
  const KURL url1 = KURL("http://test/resource1");
  Persistent<FakeResource> resource1 =
      MakeGarbageCollected<FakeResource>(url1, ResourceType::kRaw);
  GetMemoryCache()->Add(resource1);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource1));

  GetMemoryCache()->RemoveURLFromCache(url1);
  EXPECT_FALSE(GetMemoryCache()->Contains(resource1));

  const KURL url2 = KURL("http://test/resource2#foo");
  auto* resource2 =
      MakeGarbageCollected<FakeResource>(url2, ResourceType::kRaw);
  GetMemoryCache()->Add(resource2);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource2));

  GetMemoryCache()->RemoveURLFromCache(url2);
  EXPECT_FALSE(GetMemoryCache()->Contains(resource2));
}

}  // namespace blink
