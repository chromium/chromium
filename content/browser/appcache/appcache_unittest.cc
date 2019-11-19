// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

class AppCacheTest : public testing::Test {
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AppCacheTest, CleanupUnusedCache) {
  MockAppCacheService service;
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 111);
  cache->set_complete(true);
  auto group = base::MakeRefCounted<AppCacheGroup>(
      service.storage(), GURL("http://blah/manifest"), 111);
  group->AddCache(cache.get());

  mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend1;
  ignore_result(frontend1.InitWithNewPipeAndPassReceiver());
  AppCacheHost host1(/*host_id=*/base::UnguessableToken::Create(),
                     /*process_id=*/1, /*render_frame_id=*/1,
                     std::move(frontend1), &service);

  mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend2;
  ignore_result(frontend2.InitWithNewPipeAndPassReceiver());
  AppCacheHost host2(/*host_id=*/base::UnguessableToken::Create(),
                     /*process_id=*/2, /*render_frame_id=*/2,
                     std::move(frontend2), &service);

  host1.AssociateCompleteCache(cache.get());
  host2.AssociateCompleteCache(cache.get());

  host1.AssociateNoCache(GURL());
  host2.AssociateNoCache(GURL());
}

TEST_F(AppCacheTest, AddModifyRemoveEntry) {
  MockAppCacheService service;
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 111);

  EXPECT_TRUE(cache->entries().empty());
  EXPECT_EQ(0L, cache->cache_size());
  EXPECT_EQ(0L, cache->padding_size());

  const GURL kFooUrl("http://foo.com");
  const int64_t kFooResponseId = 1;
  const int64_t kFooSize = 100;
  AppCacheEntry entry1(AppCacheEntry::MASTER, kFooResponseId,
                       /*response_size=*/kFooSize,
                       /*padding_size=*/0);
  cache->AddEntry(kFooUrl, entry1);
  EXPECT_EQ(entry1.types(), cache->GetEntry(kFooUrl)->types());
  EXPECT_EQ(1UL, cache->entries().size());
  EXPECT_EQ(kFooSize, cache->cache_size());
  EXPECT_EQ(0L, cache->padding_size());

  const GURL kBarUrl("http://bar.com");
  const int64_t kBarResponseId = 2;
  const int64_t kBarSize = 200;
  AppCacheEntry entry2(AppCacheEntry::FALLBACK, kBarResponseId,
                       /*response_size=*/kBarSize,
                       /*padding_size=*/2 * kBarSize);
  EXPECT_TRUE(cache->AddOrModifyEntry(kBarUrl, entry2));
  EXPECT_EQ(entry2.types(), cache->GetEntry(kBarUrl)->types());
  EXPECT_EQ(2UL, cache->entries().size());
  EXPECT_EQ(kFooSize + kBarSize, cache->cache_size());
  EXPECT_EQ(2 * kBarSize, cache->padding_size());

  // Expected to return false when an existing entry is modified.
  AppCacheEntry entry3(AppCacheEntry::EXPLICIT);
  EXPECT_FALSE(cache->AddOrModifyEntry(kFooUrl, entry3));
  EXPECT_EQ((AppCacheEntry::MASTER | AppCacheEntry::EXPLICIT),
            cache->GetEntry(kFooUrl)->types());
  // Only the type should be modified.
  EXPECT_EQ(kFooResponseId, cache->GetEntry(kFooUrl)->response_id());
  EXPECT_EQ(kFooSize, cache->GetEntry(kFooUrl)->response_size());
  EXPECT_EQ(kFooSize + kBarSize, cache->cache_size());
  EXPECT_EQ(2 * kBarSize, cache->padding_size());

  EXPECT_EQ(entry2.types(), cache->GetEntry(kBarUrl)->types());  // unchanged

  cache->RemoveEntry(kBarUrl);
  EXPECT_EQ(kFooSize, cache->cache_size());
  cache->RemoveEntry(kFooUrl);
  EXPECT_EQ(0L, cache->cache_size());
  EXPECT_EQ(0L, cache->padding_size());
  EXPECT_TRUE(cache->entries().empty());
}

TEST_F(AppCacheTest, InitializeWithManifest) {
  MockAppCacheService service;

  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 1234);
  EXPECT_TRUE(cache->fallback_namespaces_.empty());
  EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
  EXPECT_FALSE(cache->online_whitelist_all_);

  AppCacheManifest manifest;
  manifest.explicit_urls.insert("http://one.com");
  manifest.explicit_urls.insert("http://two.com");
  manifest.fallback_namespaces.push_back(
      AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE, GURL("http://fb1.com"),
                GURL("http://fbone.com"), true));
  manifest.online_whitelist_namespaces.push_back(
      AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE, GURL("http://w1.com"),
          GURL(), false));
  manifest.online_whitelist_namespaces.push_back(
      AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE, GURL("http://w2.com"),
          GURL(), false));
  manifest.online_whitelist_all = true;

  cache->InitializeWithManifest(&manifest);
  const std::vector<AppCacheNamespace>& fallbacks =
      cache->fallback_namespaces_;
  size_t expected = 1;
  EXPECT_EQ(expected, fallbacks.size());
  EXPECT_EQ(GURL("http://fb1.com"), fallbacks[0].namespace_url);
  EXPECT_EQ(GURL("http://fbone.com"), fallbacks[0].target_url);
  EXPECT_TRUE(fallbacks[0].is_pattern);
  const std::vector<AppCacheNamespace>& whitelist =
      cache->online_whitelist_namespaces_;
  expected = 2;
  EXPECT_EQ(expected, whitelist.size());
  EXPECT_EQ(GURL("http://w1.com"), whitelist[0].namespace_url);
  EXPECT_EQ(GURL("http://w2.com"), whitelist[1].namespace_url);
  EXPECT_TRUE(cache->online_whitelist_all_);

  // Ensure collections in manifest were taken over by the cache rather than
  // copied.
  EXPECT_TRUE(manifest.fallback_namespaces.empty());
  EXPECT_TRUE(manifest.online_whitelist_namespaces.empty());
}

TEST_F(AppCacheTest, FindResponseForRequest) {
  MockAppCacheService service;

  const GURL kOnlineNamespaceUrl("http://blah/online_namespace");
  const GURL kFallbackEntryUrl1("http://blah/fallback_entry1");
  const GURL kFallbackNamespaceUrl1("http://blah/fallback_namespace/");
  const GURL kFallbackEntryUrl2("http://blah/fallback_entry2");
  const GURL kFallbackNamespaceUrl2("http://blah/fallback_namespace/longer");
  const GURL kManifestUrl("http://blah/manifest");
  const GURL kForeignExplicitEntryUrl("http://blah/foreign");
  const GURL kInOnlineNamespaceUrl(
      "http://blah/online_namespace/network");
  const GURL kExplicitInOnlineNamespaceUrl(
      "http://blah/online_namespace/explicit");
  const GURL kFallbackTestUrl1("http://blah/fallback_namespace/1");
  const GURL kFallbackTestUrl2("http://blah/fallback_namespace/longer2");
  const GURL kInterceptNamespace("http://blah/intercept_namespace/");
  const GURL kInterceptNamespaceWithinFallback(
      "http://blah/fallback_namespace/intercept_namespace/");
  const GURL kInterceptNamespaceEntry("http://blah/intercept_entry");
  const GURL kOnlineNamespaceWithinOtherNamespaces(
      "http://blah/fallback_namespace/intercept_namespace/1/online");

  const int64_t kFallbackResponseId1 = 1;
  const int64_t kFallbackResponseId2 = 2;
  const int64_t kManifestResponseId = 3;
  const int64_t kForeignExplicitResponseId = 4;
  const int64_t kExplicitInOnlineNamespaceResponseId = 5;
  const int64_t kInterceptResponseId = 6;

  AppCacheManifest manifest;
  manifest.online_whitelist_namespaces.push_back(
      AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE, kOnlineNamespaceUrl,
          GURL(), false));
  manifest.online_whitelist_namespaces.push_back(
      AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE,
          kOnlineNamespaceWithinOtherNamespaces, GURL(), false));
  manifest.fallback_namespaces.push_back(
      AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE, kFallbackNamespaceUrl1,
          kFallbackEntryUrl1, false));
  manifest.fallback_namespaces.push_back(
      AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE, kFallbackNamespaceUrl2,
          kFallbackEntryUrl2, false));
  manifest.intercept_namespaces.push_back(
      AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE, kInterceptNamespace,
          kInterceptNamespaceEntry, false));
  manifest.intercept_namespaces.push_back(
      AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE,
          kInterceptNamespaceWithinFallback, kInterceptNamespaceEntry, false));

  // Create a cache with some namespaces and entries.
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 1234);
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kFallbackEntryUrl1,
      AppCacheEntry(AppCacheEntry::FALLBACK, kFallbackResponseId1));
  cache->AddEntry(
      kFallbackEntryUrl2,
      AppCacheEntry(AppCacheEntry::FALLBACK, kFallbackResponseId2));
  cache->AddEntry(
      kManifestUrl,
      AppCacheEntry(AppCacheEntry::MANIFEST, kManifestResponseId));
  cache->AddEntry(
      kForeignExplicitEntryUrl,
      AppCacheEntry(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
                    kForeignExplicitResponseId));
  cache->AddEntry(
      kExplicitInOnlineNamespaceUrl,
      AppCacheEntry(AppCacheEntry::EXPLICIT,
                    kExplicitInOnlineNamespaceResponseId));
  cache->AddEntry(
      kInterceptNamespaceEntry,
      AppCacheEntry(AppCacheEntry::INTERCEPT, kInterceptResponseId));
  cache->set_complete(true);

  // See that we get expected results from FindResponseForRequest

  bool found = false;
  AppCacheEntry entry;
  AppCacheEntry fallback_entry;
  GURL intercept_namespace;
  GURL fallback_namespace;
  bool network_namespace = false;

  found = cache->FindResponseForRequest(GURL("http://blah/miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(kForeignExplicitEntryUrl,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kForeignExplicitResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kManifestUrl,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kManifestResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kInOnlineNamespaceUrl,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(network_namespace);

  network_namespace = false;  // reset

  found = cache->FindResponseForRequest(kExplicitInOnlineNamespaceUrl,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kExplicitInOnlineNamespaceResponseId, entry.response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kFallbackTestUrl1,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId1, fallback_entry.response_id());
  EXPECT_EQ(kFallbackEntryUrl1,
            cache->GetFallbackEntryUrl(fallback_namespace));
  EXPECT_FALSE(network_namespace);

  fallback_entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kFallbackTestUrl2,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId2, fallback_entry.response_id());
  EXPECT_EQ(kFallbackEntryUrl2,
            cache->GetFallbackEntryUrl(fallback_namespace));
  EXPECT_FALSE(network_namespace);

  fallback_entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(kOnlineNamespaceWithinOtherNamespaces,
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(network_namespace);

  fallback_entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(
      kOnlineNamespaceWithinOtherNamespaces.Resolve("online_resource"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(network_namespace);

  fallback_namespace = GURL();

  found = cache->FindResponseForRequest(
      kInterceptNamespace.Resolve("intercept_me"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kInterceptResponseId, entry.response_id());
  EXPECT_EQ(kInterceptNamespaceEntry,
            cache->GetInterceptEntryUrl(intercept_namespace));
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(fallback_namespace.is_empty());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(
      kInterceptNamespaceWithinFallback.Resolve("intercept_me"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kInterceptResponseId, entry.response_id());
  EXPECT_EQ(kInterceptNamespaceEntry,
            cache->GetInterceptEntryUrl(intercept_namespace));
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(fallback_namespace.is_empty());
  EXPECT_FALSE(network_namespace);
}

TEST_F(AppCacheTest, FindInterceptPatternResponseForRequest) {
  MockAppCacheService service;

  // Setup an appcache with an intercept namespace that uses pattern matching.
  const GURL kInterceptNamespaceBase("http://blah/intercept_namespace/");
  const GURL kInterceptPatternNamespace(
      kInterceptNamespaceBase.Resolve("*.hit*"));
  const GURL kInterceptNamespaceEntry("http://blah/intercept_resource");
  const int64_t kInterceptResponseId = 1;
  AppCacheManifest manifest;
  manifest.intercept_namespaces.push_back(
      AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE,
          kInterceptPatternNamespace, kInterceptNamespaceEntry, true));
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 1234);
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kInterceptNamespaceEntry,
      AppCacheEntry(AppCacheEntry::INTERCEPT, kInterceptResponseId));
  cache->set_complete(true);

  // See that the pattern match works.
  bool found = false;
  AppCacheEntry entry;
  AppCacheEntry fallback_entry;
  GURL intercept_namespace;
  GURL fallback_namespace;
  bool network_namespace = false;

  found = cache->FindResponseForRequest(
      GURL("http://blah/miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(
      GURL("http://blah/intercept_namespace/another_miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(
      GURL("http://blah/intercept_namespace/path.hit"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kInterceptResponseId, entry.response_id());
  EXPECT_EQ(kInterceptNamespaceEntry,
            cache->GetInterceptEntryUrl(intercept_namespace));
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(fallback_namespace.is_empty());
  EXPECT_FALSE(network_namespace);

  entry = AppCacheEntry();  // reset

  found = cache->FindResponseForRequest(
      GURL("http://blah/intercept_namespace/longer/path.hit?arg=ok"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_EQ(kInterceptResponseId, entry.response_id());
  EXPECT_EQ(kInterceptNamespaceEntry,
            cache->GetInterceptEntryUrl(intercept_namespace));
  EXPECT_FALSE(fallback_entry.has_response_id());
  EXPECT_TRUE(fallback_namespace.is_empty());
  EXPECT_FALSE(network_namespace);
}

TEST_F(AppCacheTest, FindFallbackPatternResponseForRequest) {
  MockAppCacheService service;

  // Setup an appcache with a fallback namespace that uses pattern matching.
  const GURL kFallbackNamespaceBase("http://blah/fallback_namespace/");
  const GURL kFallbackPatternNamespace(
      kFallbackNamespaceBase.Resolve("*.hit*"));
  const GURL kFallbackNamespaceEntry("http://blah/fallback_resource");
  const int64_t kFallbackResponseId = 1;
  AppCacheManifest manifest;
  manifest.fallback_namespaces.push_back(
      AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE, kFallbackPatternNamespace,
                kFallbackNamespaceEntry, true));
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 1234);
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kFallbackNamespaceEntry,
      AppCacheEntry(AppCacheEntry::FALLBACK, kFallbackResponseId));
  cache->set_complete(true);

  // See that the pattern match works.
  bool found = false;
  AppCacheEntry entry;
  AppCacheEntry fallback_entry;
  GURL intercept_namespace;
  GURL fallback_namespace;
  bool network_namespace = false;

  found = cache->FindResponseForRequest(
      GURL("http://blah/miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(
      GURL("http://blah/fallback_namespace/another_miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(
      GURL("http://blah/fallback_namespace/path.hit"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId, fallback_entry.response_id());
  EXPECT_EQ(kFallbackNamespaceEntry,
            cache->GetFallbackEntryUrl(fallback_namespace));
  EXPECT_FALSE(network_namespace);

  fallback_entry = AppCacheEntry();
  fallback_namespace = GURL();

  found = cache->FindResponseForRequest(
      GURL("http://blah/fallback_namespace/longer/path.hit?arg=ok"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_EQ(kFallbackResponseId, fallback_entry.response_id());
  EXPECT_EQ(kFallbackNamespaceEntry,
            cache->GetFallbackEntryUrl(fallback_namespace));
  EXPECT_TRUE(intercept_namespace.is_empty());
  EXPECT_FALSE(network_namespace);
}

TEST_F(AppCacheTest, FindNetworkNamespacePatternResponseForRequest) {
  MockAppCacheService service;

  // Setup an appcache with a network namespace that uses pattern matching.
  const GURL kNetworkNamespaceBase("http://blah/network_namespace/");
  const GURL kNetworkPatternNamespace(
      kNetworkNamespaceBase.Resolve("*.hit*"));
  AppCacheManifest manifest;
  manifest.online_whitelist_namespaces.push_back(
      AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE, kNetworkPatternNamespace,
                GURL(), true));
  manifest.online_whitelist_all = false;
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), 1234);
  cache->InitializeWithManifest(&manifest);
  cache->set_complete(true);

  // See that the pattern match works.
  bool found = false;
  AppCacheEntry entry;
  AppCacheEntry fallback_entry;
  GURL intercept_namespace;
  GURL fallback_namespace;
  bool network_namespace = false;

  found = cache->FindResponseForRequest(
      GURL("http://blah/miss"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_FALSE(found);

  found = cache->FindResponseForRequest(
      GURL("http://blah/network_namespace/path.hit"),
      &entry, &intercept_namespace,
      &fallback_entry, &fallback_namespace,
      &network_namespace);
  EXPECT_TRUE(found);
  EXPECT_TRUE(network_namespace);
  EXPECT_FALSE(entry.has_response_id());
  EXPECT_FALSE(fallback_entry.has_response_id());
}

TEST_F(AppCacheTest, ToFromDatabaseRecords) {
  // Setup a cache with some entries.
  const int64_t kCacheId = 1234;
  const int64_t kGroupId = 4321;
  const GURL kManifestUrl("http://foo.com/manifest");
  const GURL kInterceptUrl("http://foo.com/intercept.html");
  const GURL kFallbackUrl("http://foo.com/fallback.html");
  const GURL kWhitelistUrl("http://foo.com/whitelist*");
  const std::string kData(
    "CACHE MANIFEST\r"
    "CHROMIUM-INTERCEPT:\r"
    "/intercept return /intercept.html\r"
    "FALLBACK:\r"
    "/ /fallback.html\r"
    "NETWORK:\r"
    "/whitelist* isPattern\r"
    "*\r");
  MockAppCacheService service;
  auto cache = base::MakeRefCounted<AppCache>(service.storage(), kCacheId);
  auto group = base::MakeRefCounted<AppCacheGroup>(service.storage(),
                                                   kManifestUrl, kGroupId);
  AppCacheManifest manifest;
  EXPECT_TRUE(ParseManifest(kManifestUrl, kData.c_str(), kData.length(),
                            PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES,
                            manifest));
  cache->InitializeWithManifest(&manifest);
  EXPECT_EQ(APPCACHE_NETWORK_NAMESPACE,
            cache->online_whitelist_namespaces_[0].type);
  EXPECT_TRUE(cache->online_whitelist_namespaces_[0].is_pattern);
  EXPECT_EQ(kWhitelistUrl,
            cache->online_whitelist_namespaces_[0].namespace_url);
  cache->AddEntry(kManifestUrl, AppCacheEntry(AppCacheEntry::MANIFEST,
                                              /*response_id=*/1,
                                              /*response_size=*/1000,
                                              /*padding_size=*/0));
  cache->AddEntry(kInterceptUrl, AppCacheEntry(AppCacheEntry::INTERCEPT,
                                               /*response_id=*/3,
                                               /*response_size=*/10000,
                                               /*padding_size=*/10));
  cache->AddEntry(kFallbackUrl, AppCacheEntry(AppCacheEntry::FALLBACK,
                                              /*response_id=*/2,
                                              /*response_size=*/100000,
                                              /*padding_size=*/100));

  // Get it to produce database records and verify them.
  AppCacheDatabase::CacheRecord cache_record;
  std::vector<AppCacheDatabase::EntryRecord> entries;
  std::vector<AppCacheDatabase::NamespaceRecord> intercepts;
  std::vector<AppCacheDatabase::NamespaceRecord> fallbacks;
  std::vector<AppCacheDatabase::OnlineWhiteListRecord> whitelists;
  cache->ToDatabaseRecords(group.get(),
                           &cache_record,
                           &entries,
                           &intercepts,
                           &fallbacks,
                           &whitelists);
  EXPECT_EQ(kCacheId, cache_record.cache_id);
  EXPECT_EQ(kGroupId, cache_record.group_id);
  EXPECT_TRUE(cache_record.online_wildcard);
  EXPECT_EQ(1000 + 10000 + 100000, cache_record.cache_size);
  EXPECT_EQ(0 + 10 + 100, cache_record.padding_size);
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(1u, intercepts.size());
  EXPECT_EQ(1u, fallbacks.size());
  EXPECT_EQ(1u, whitelists.size());
  cache = nullptr;

  // Create a new AppCache and populate it with those records and verify.
  cache = base::MakeRefCounted<AppCache>(service.storage(), kCacheId);
  cache->InitializeWithDatabaseRecords(
      cache_record, entries, intercepts,
      fallbacks, whitelists);
  EXPECT_TRUE(cache->online_whitelist_all_);
  EXPECT_EQ(3u, cache->entries().size());
  EXPECT_TRUE(cache->GetEntry(kManifestUrl));
  EXPECT_TRUE(cache->GetEntry(kInterceptUrl));
  EXPECT_TRUE(cache->GetEntry(kFallbackUrl));
  EXPECT_EQ(kInterceptUrl,
            cache->GetInterceptEntryUrl(GURL("http://foo.com/intercept")));
  EXPECT_EQ(kFallbackUrl,
            cache->GetFallbackEntryUrl(GURL("http://foo.com/")));
  EXPECT_EQ(1000 + 10000 + 100000, cache->cache_size());
  EXPECT_EQ(0 + 10 + 100, cache->padding_size());
  EXPECT_EQ(APPCACHE_NETWORK_NAMESPACE,
            cache->online_whitelist_namespaces_[0].type);
  EXPECT_TRUE(cache->online_whitelist_namespaces_[0].is_pattern);
  EXPECT_EQ(kWhitelistUrl,
            cache->online_whitelist_namespaces_[0].namespace_url);
}

TEST_F(AppCacheTest, IsNamespaceMatch) {
  AppCacheNamespace prefix;
  prefix.namespace_url = GURL("http://foo.com/prefix");
  prefix.is_pattern = false;
  EXPECT_TRUE(prefix.IsMatch(
      GURL("http://foo.com/prefix_and_anothing_goes")));
  EXPECT_FALSE(prefix.IsMatch(
      GURL("http://foo.com/nope")));

  AppCacheNamespace bar_no_star;
  bar_no_star.namespace_url = GURL("http://foo.com/bar");
  bar_no_star.is_pattern = true;
  EXPECT_TRUE(bar_no_star.IsMatch(
      GURL("http://foo.com/bar")));
  EXPECT_FALSE(bar_no_star.IsMatch(
      GURL("http://foo.com/bar/nope")));

  AppCacheNamespace bar_star;
  bar_star.namespace_url = GURL("http://foo.com/bar/*");
  bar_star.is_pattern = true;
  EXPECT_TRUE(bar_star.IsMatch(
      GURL("http://foo.com/bar/")));
  EXPECT_TRUE(bar_star.IsMatch(
      GURL("http://foo.com/bar/should_match")));
  EXPECT_FALSE(bar_star.IsMatch(
      GURL("http://foo.com/not_bar/should_not_match")));

  AppCacheNamespace star_bar_star;
  star_bar_star.namespace_url = GURL("http://foo.com/*/bar/*");
  star_bar_star.is_pattern = true;
  EXPECT_TRUE(star_bar_star.IsMatch(
      GURL("http://foo.com/any/bar/should_match")));
  EXPECT_TRUE(star_bar_star.IsMatch(
      GURL("http://foo.com/any/bar/")));
  EXPECT_FALSE(star_bar_star.IsMatch(
      GURL("http://foo.com/any/not_bar/no_match")));

  AppCacheNamespace query_star_edit;
  query_star_edit.namespace_url = GURL("http://foo.com/query?id=*&verb=edit*");
  query_star_edit.is_pattern = true;
  EXPECT_TRUE(query_star_edit.IsMatch(
      GURL("http://foo.com/query?id=1234&verb=edit&option=blue")));
  EXPECT_TRUE(query_star_edit.IsMatch(
      GURL("http://foo.com/query?id=12345&option=blue&verb=edit")));
  EXPECT_FALSE(query_star_edit.IsMatch(
      GURL("http://foo.com/query?id=12345&option=blue&verb=print")));
  EXPECT_TRUE(query_star_edit.IsMatch(
      GURL("http://foo.com/query?id=123&verb=print&verb=edit")));

  AppCacheNamespace star_greediness;
  star_greediness.namespace_url = GURL("http://foo.com/*/b");
  star_greediness.is_pattern = true;
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/wxy/z/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/b/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/b/b/b/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/b/b/a/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/01234567890abcdef/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/01234567890abcdef/b01234567890abcdef/b")));
  EXPECT_TRUE(star_greediness.IsMatch(
      GURL("http://foo.com/a/b/01234567890abcdef_eat_some_more_characters_"
           "/and_even_more_for_the_heck_of_it/01234567890abcdef/b")));
}

}  // namespace content
