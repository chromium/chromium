// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/mock_http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

struct CacheTestEntry {
  const char* url;
  const char* date;
};

HttpCacheDataRemover::HttpCacheDataRemoverCallback
MakeHttpCacheDataRemoverCallback(base::OnceClosure callback) {
  return base::BindOnce(
      [](base::OnceClosure callback, HttpCacheDataRemover* remover) {
        std::move(callback).Run();
      },
      std::move(callback));
}

constexpr CacheTestEntry kCacheEntries[] = {
    {"http://www.google.com", "15 Jun 1975"},
    {"https://www.google.com", "15 Jun 1985"},
    {"http://www.wikipedia.com", "15 Jun 1995"},
    {"https://www.wikipedia.com", "15 Jun 2005"},
    {"http://localhost:1234/mysite", "15 Jun 2015"},
    {"https://localhost:1234/mysite", "15 Jun 2016"},
    {"http://localhost:3456/yoursite", "15 Jun 2017"},
    {"https://localhost:3456/yoursite", "15 Jun 2018"}};

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

class HttpCacheDataRemoverTest : public testing::Test {
 public:
  HttpCacheDataRemoverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {}

  ~HttpCacheDataRemoverTest() override = default;

  void SetUp() override {
    InitNetworkContext();

    cache_ = network_context_->url_request_context()
                 ->http_transaction_factory()
                 ->GetCache();
    ASSERT_TRUE(cache_);
    {
      net::TestGetBackendCompletionCallback callback;
      net::HttpCache::GetBackendResult result =
          cache_->GetBackend(callback.callback());
      result = callback.GetResult(result);
      ASSERT_EQ(net::OK, result.first);
      backend_ = result.second;
      ASSERT_TRUE(backend_);
    }

    // Create some entries in the cache.
    for (const CacheTestEntry& test_entry : kCacheEntries) {
      TestEntryResultCompletionCallback callback;
      std::string key = ComputeCacheKey(test_entry.url);
      disk_cache::EntryResult result =
          backend_->CreateEntry(key, net::HIGHEST, callback.callback());
      result = callback.GetResult(std::move(result));
      ASSERT_EQ(net::OK, result.net_error());
      disk_cache::Entry* entry = result.ReleaseEntry();
      ASSERT_TRUE(entry);
      base::Time time;
      ASSERT_TRUE(base::Time::FromString(test_entry.date, &time));
      entry->SetLastUsedTimeForTest(time);
      entry->Close();
      task_environment_.RunUntilIdle();
    }
    ASSERT_EQ(std::size(kCacheEntries),
              static_cast<size_t>(backend_->GetEntryCount()));
  }

  std::string ComputeCacheKey(const std::string& url_string) {
    GURL url(url_string);
    const auto kOrigin = url::Origin::Create(url);
    const net::SchemefulSite kSite = net::SchemefulSite(kOrigin);
    net::HttpRequestInfo request_info;
    request_info.url = url;
    request_info.method = "GET";
    request_info.network_isolation_key = net::NetworkIsolationKey(kSite, kSite);
    request_info.network_anonymization_key =
        net::NetworkAnonymizationKey::CreateSameSite(kSite);
    return *net::HttpCache::GenerateCacheKeyForRequest(&request_info);
  }

  void RemoveData(mojom::ClearDataFilterPtr filter,
                  base::Time start_time,
                  base::Time end_time) {
    base::RunLoop run_loop;
    std::unique_ptr<HttpCacheDataRemover> data_remover =
        HttpCacheDataRemover::CreateAndStart(
            network_context_->url_request_context(), std::move(filter),
            start_time, end_time,
            MakeHttpCacheDataRemoverCallback(run_loop.QuitClosure()));
    run_loop.Run();
  }

  bool HasEntry(const std::string& url_string) {
    std::string key = ComputeCacheKey(url_string);
    base::RunLoop run_loop;
    TestEntryResultCompletionCallback callback;
    disk_cache::EntryResult result =
        backend_->OpenEntry(key, net::HIGHEST, callback.callback());
    if (result.net_error() == net::ERR_IO_PENDING)
      result = callback.WaitForResult();
    disk_cache::Entry* entry = result.ReleaseEntry();
    if (entry)
      entry->Close();
    return entry != nullptr;
  }

 protected:
  void InitNetworkContext() {
    backend_ = nullptr;
    cache_ = nullptr;
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->http_cache_enabled = true;
    network_context_remote_.reset();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;

  // Stores the mojo::Remote<NetworkContext> of the most recently created
  // NetworkContext.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  raw_ptr<disk_cache::Backend> backend_ = nullptr;

 private:
  raw_ptr<net::HttpCache> cache_;
};

class HttpCacheDataRemoverSplitCacheTest : public HttpCacheDataRemoverTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
    HttpCacheDataRemoverTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(HttpCacheDataRemoverTest, ClearAll) {
  EXPECT_NE(0, backend_->GetEntryCount());
  RemoveData(/*url_filter=*/nullptr, base::Time(), base::Time());
  EXPECT_EQ(0, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterDeleteByDomain) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->domains.push_back("google.com");
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[0].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[1].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[2].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[3].url));
  EXPECT_EQ(4, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterKeepByDomain) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->domains.push_back("google.com");
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_TRUE(HasEntry(kCacheEntries[0].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[1].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[2].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[3].url));
  EXPECT_EQ(4, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterDeleteByOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->origins.push_back(url::Origin::Create(GURL("http://www.google.com")));
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[0].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(6, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterKeepByOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->origins.push_back(url::Origin::Create(GURL("http://www.google.com")));
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_TRUE(HasEntry(kCacheEntries[0].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(2, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterDeleteByDomainAndOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[2].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[3].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(5, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterKeepByDomainAndOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_TRUE(HasEntry(kCacheEntries[2].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[3].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(3, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterByDateFromUnbounded) {
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[5].date, &end_time));
  RemoveData(/*filter=*/nullptr, base::Time(), end_time);
  EXPECT_TRUE(HasEntry(kCacheEntries[5].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[6].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[7].url));
  EXPECT_EQ(3, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterByDateToUnbounded) {
  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[5].date, &start_time));
  RemoveData(/*filter=*/nullptr, start_time, base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[5].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[6].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[7].url));
  EXPECT_EQ(5, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterByDateRange) {
  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[1].date, &start_time));
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[6].date, &end_time));
  RemoveData(/*filter=*/nullptr, start_time, end_time);
  EXPECT_FALSE(HasEntry(kCacheEntries[1].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[2].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[3].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[4].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[5].url));
  EXPECT_EQ(3, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterDeleteByDomainAndDate) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->domains.push_back("google.com");
  filter->domains.push_back("wikipedia.com");

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[1].date, &start_time));
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[6].date, &end_time));

  RemoveData(std::move(filter), start_time, end_time);
  EXPECT_FALSE(HasEntry(kCacheEntries[1].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[2].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[3].url));
  EXPECT_EQ(5, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, FilterKeepByDomainAndDate) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("google.com");
  filter->domains.push_back("wikipedia.com");

  base::Time start_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[1].date, &start_time));
  base::Time end_time;
  ASSERT_TRUE(base::Time::FromString(kCacheEntries[6].date, &end_time));

  RemoveData(std::move(filter), start_time, end_time);
  EXPECT_FALSE(HasEntry(kCacheEntries[4].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[5].url));
  EXPECT_EQ(6, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverTest, DeleteHttpRemover) {
  bool callback_invoked = false;
  std::unique_ptr<HttpCacheDataRemover> data_remover =
      HttpCacheDataRemover::CreateAndStart(
          network_context_->url_request_context(),
          /*url_filter=*/nullptr, base::Time(), base::Time(),
          MakeHttpCacheDataRemoverCallback(base::BindOnce(
              [](bool* callback_invoked) { *callback_invoked = true; },
              &callback_invoked)));
  // Delete the data remover and make sure after all task have been processed
  // that the callback wasn't invoked.
  data_remover.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_invoked);
}

// This test exercises the different code paths wrt whether the cache's backend
// is available when clearing the cache.
TEST_F(HttpCacheDataRemoverTest, TestDelayedBackend) {
  // Reinit the network context without retrieving the cache's backend so the
  // call to clear the cache does it.
  InitNetworkContext();

  net::HttpCache* http_cache = network_context_->url_request_context()
                                   ->http_transaction_factory()
                                   ->GetCache();
  ASSERT_TRUE(http_cache);
  ASSERT_FALSE(http_cache->GetCurrentBackend());
  RemoveData(/*filter=*/nullptr, base::Time(), base::Time());

  // This should have retrieved the backend of the cache.
  EXPECT_TRUE(http_cache->GetCurrentBackend());
  // Clear again the cache to test that it works when the backend is readily
  // available.
  RemoveData(/*filter=*/nullptr, base::Time(), base::Time());
}

TEST_F(HttpCacheDataRemoverSplitCacheTest, FilterDeleteByDomain) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->domains.push_back("google.com");
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[0].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[1].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[2].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[3].url));
  EXPECT_EQ(4, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverSplitCacheTest, FilterKeepByDomain) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->domains.push_back("wikipedia.com");
  filter->domains.push_back("google.com");
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_TRUE(HasEntry(kCacheEntries[0].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[1].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[2].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[3].url));
  EXPECT_EQ(4, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverSplitCacheTest, FilterDeleteByOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::DELETE_MATCHES;
  filter->origins.push_back(url::Origin::Create(GURL("http://www.google.com")));
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_FALSE(HasEntry(kCacheEntries[0].url));
  EXPECT_FALSE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(6, backend_->GetEntryCount());
}

TEST_F(HttpCacheDataRemoverSplitCacheTest, FilterKeepByOrigin) {
  mojom::ClearDataFilterPtr filter = mojom::ClearDataFilter::New();
  filter->type = mojom::ClearDataFilter_Type::KEEP_MATCHES;
  filter->origins.push_back(url::Origin::Create(GURL("http://www.google.com")));
  filter->origins.push_back(url::Origin::Create(GURL("http://localhost:1234")));
  RemoveData(std::move(filter), base::Time(), base::Time());
  EXPECT_TRUE(HasEntry(kCacheEntries[0].url));
  EXPECT_TRUE(HasEntry(kCacheEntries[4].url));
  EXPECT_EQ(2, backend_->GetEntryCount());
}

}  // namespace

}  // namespace network
