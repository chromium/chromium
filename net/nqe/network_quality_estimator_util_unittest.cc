// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_estimator_util.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::nqe::internal {

namespace {

#if BUILDFLAG(IS_IOS)
// Flaky on iOS: crbug.com/672917.
#define MAYBE_ReservedHost DISABLED_ReservedHost
#else
#define MAYBE_ReservedHost ReservedHost
#endif
// Verify that the cached network qualities from the prefs are not used if the
// reading of the network quality prefs is not enabled..
TEST(NetworkQualityEstimatorUtilTest, MAYBE_ReservedHost) {
  base::test::TaskEnvironment task_environment;

  MockCachingHostResolver mock_host_resolver;

  // example1.com resolves to a private IP address.
  mock_host_resolver.rules()->AddRule("example1.com", "127.0.0.3");

  // example2.com resolves to a public IP address.
  mock_host_resolver.rules()->AddRule("example2.com", "27.0.0.3");

  EXPECT_EQ(0u, mock_host_resolver.num_resolve());

  // Load hostnames into HostResolver cache.
  int rv = mock_host_resolver.LoadIntoCache(
      url::SchemeHostPort("https", "example1.com", 443),
      NetworkAnonymizationKey(), std::nullopt);
  EXPECT_EQ(OK, rv);
  rv = mock_host_resolver.LoadIntoCache(
      url::SchemeHostPort("https", "example2.com", 443),
      NetworkAnonymizationKey(), std::nullopt);
  EXPECT_EQ(OK, rv);

  EXPECT_EQ(2u, mock_host_resolver.num_non_local_resolves());

  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver,
      url::SchemeHostPort("http", "[2607:f8b0:4006:819::200e]", 80),
      NetworkAnonymizationKey()));

  EXPECT_TRUE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "192.168.0.1", 443),
      NetworkAnonymizationKey()));

  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "92.168.0.1", 443),
      NetworkAnonymizationKey()));

  EXPECT_TRUE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example1.com", 443),
      NetworkAnonymizationKey()));

  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example2.com", 443),
      NetworkAnonymizationKey()));

  // IsPrivateHostForTesting() should have queried only the resolver's cache.
  EXPECT_EQ(2u, mock_host_resolver.num_non_local_resolves());
}

#if BUILDFLAG(IS_IOS)
// Flaky on iOS: crbug.com/672917.
#define MAYBE_ReservedHostUncached DISABLED_ReservedHostUncached
#else
#define MAYBE_ReservedHostUncached ReservedHostUncached
#endif
// Verify that IsPrivateHostForTesting() returns false for a hostname whose DNS
// resolution is not cached. Further, once the resolution is cached, verify that
// the cached entry is used.
TEST(NetworkQualityEstimatorUtilTest, MAYBE_ReservedHostUncached) {
  base::test::TaskEnvironment task_environment;

  MockCachingHostResolver mock_host_resolver;

  auto rules = base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);

  // Add example3.com resolution to the DNS cache.
  mock_host_resolver.rules()->AddRule("example3.com", "127.0.0.3");

  // Not in DNS host cache, so should not be marked as private.
  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example3.com", 443),
      NetworkAnonymizationKey()));
  EXPECT_EQ(0u, mock_host_resolver.num_non_local_resolves());

  int rv = mock_host_resolver.LoadIntoCache(
      url::SchemeHostPort("https", "example3.com", 443),
      NetworkAnonymizationKey(), std::nullopt);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(1u, mock_host_resolver.num_non_local_resolves());

  EXPECT_TRUE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example3.com", 443),
      NetworkAnonymizationKey()));

  // IsPrivateHostForTesting() should have queried only the resolver's cache.
  EXPECT_EQ(1u, mock_host_resolver.num_non_local_resolves());
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Flaky on iOS: crbug.com/672917.
// Flaky on Android: crbug.com/1223950
#define MAYBE_ReservedHostUncachedWithNetworkIsolationKey \
  DISABLED_ReservedHostUncachedWithNetworkIsolationKey
#else
#define MAYBE_ReservedHostUncachedWithNetworkIsolationKey \
  ReservedHostUncachedWithNetworkIsolationKey
#endif
// Make sure that IsPrivateHostForTesting() uses the NetworkAnonymizationKey
// provided to it.
TEST(NetworkQualityEstimatorUtilTest,
     MAYBE_ReservedHostUncachedWithNetworkIsolationKey) {
  const SchemefulSite kSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  base::test::TaskEnvironment task_environment;

  MockCachingHostResolver mock_host_resolver;

  // Add example3.com resolution to the DNS cache.
  mock_host_resolver.rules()->AddRule("example3.com", "127.0.0.3");

  // Not in DNS host cache, so should not be marked as private.
  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example3.com", 443),
      kNetworkAnonymizationKey));
  EXPECT_EQ(0u, mock_host_resolver.num_non_local_resolves());

  int rv = mock_host_resolver.LoadIntoCache(
      url::SchemeHostPort("https", "example3.com", 443),
      kNetworkAnonymizationKey, std::nullopt);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(1u, mock_host_resolver.num_non_local_resolves());

  EXPECT_TRUE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example3.com", 443),
      kNetworkAnonymizationKey));

  // IsPrivateHostForTesting() should have queried only the resolver's cache.
  EXPECT_EQ(1u, mock_host_resolver.num_non_local_resolves());

  // IsPrivateHostForTesting should return false when using a different
  // NetworkAnonymizationKey (in this case, any empty one).
  EXPECT_FALSE(IsPrivateHostForTesting(
      &mock_host_resolver, url::SchemeHostPort("https", "example3.com", 443),
      NetworkAnonymizationKey()));
}

#if BUILDFLAG(IS_IOS)
// Flaky on iOS: crbug.com/672917.
#define MAYBE_Localhost DISABLED_Localhost
#else
#define MAYBE_Localhost Localhost
#endif

// Verify that IsPrivateHostForTesting() returns correct results for local
// hosts.
TEST(NetworkQualityEstimatorUtilTest, MAYBE_Localhost) {
  base::test::TaskEnvironment task_environment;

  // Use actual HostResolver since MockCachingHostResolver does not determine
  // the correct answer for localhosts.
  std::unique_ptr<ContextHostResolver> resolver =
      HostResolver::CreateStandaloneContextResolver(NetLog::Get());

  auto rules = base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);

  EXPECT_TRUE(IsPrivateHostForTesting(
      resolver.get(), url::SchemeHostPort("https", "localhost", 443),
      NetworkAnonymizationKey()));
  EXPECT_TRUE(IsPrivateHostForTesting(
      resolver.get(), url::SchemeHostPort("http", "127.0.0.1", 80),
      NetworkAnonymizationKey()));
  EXPECT_TRUE(IsPrivateHostForTesting(
      resolver.get(), url::SchemeHostPort("http", "0.0.0.0", 80),
      NetworkAnonymizationKey()));
  EXPECT_TRUE(IsPrivateHostForTesting(resolver.get(),
                                      url::SchemeHostPort("http", "[::1]", 80),
                                      NetworkAnonymizationKey()));
  EXPECT_FALSE(IsPrivateHostForTesting(
      resolver.get(), url::SchemeHostPort("http", "google.com", 80),
      NetworkAnonymizationKey()));

  // Legacy localhost names.
  EXPECT_FALSE(IsPrivateHostForTesting(
      resolver.get(), url::SchemeHostPort("https", "localhost6", 443),
      NetworkAnonymizationKey()));
  EXPECT_FALSE(IsPrivateHostForTesting(
      resolver.get(),
      url::SchemeHostPort("https", "localhost6.localdomain6", 443),
      NetworkAnonymizationKey()));
}

}  // namespace

}  // namespace net::nqe::internal
