// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/http_cache_data_counter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/mock_http_cache.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

struct CacheTestEntry {
  const char* url;
  const char* date;
  int size;
};

constexpr CacheTestEntry kCacheEntries[] = {
    {"http://www.google.com", "15 Jun 1975", 1024},
    {"https://www.google.com", "15 Jun 1985", 2048},
    {"http://www.wikipedia.com", "15 Jun 1995", 4096},
    {"https://www.wikipedia.com", "15 Jun 2005", 8192},
    {"http://localhost:1234/mysite", "15 Jun 2015", 16384},
    {"https://localhost:1234/mysite", "15 Jun 2016", 32768},
    {"http://localhost:3456/yoursite", "15 Jun 2017", 65536},
    {"https://localhost:3456/yoursite", "15 Jun 2018", 512}};

mojom::NetworkContextParamsPtr CreateContextParams() {
  mojom::NetworkContextParamsPtr params =
      CreateNetworkContextParamsForTesting();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

class HttpCacheDataCounterTest : public testing::Test {
 public:
  HttpCacheDataCounterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {}

  ~HttpCacheDataCounterTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(cache_dir_.CreateUniqueTempDir());
    InitNetworkContext();

    net::HttpCache* cache = network_context_->url_request_context()
                                ->http_transaction_factory()
                                ->GetCache();
    ASSERT_TRUE(cache);
    {
      net::TestGetBackendCompletionCallback callback;
      net::HttpCache::GetBackendResult result =
          cache->GetBackend(callback.callback());
      result = callback.GetResult(result);
      ASSERT_EQ(net::OK, result.first);
      backend_ = result.second;
      ASSERT_TRUE(backend_);
    }

    // Create some entries in the cache.
    for (const CacheTestEntry& test_entry : kCacheEntries) {
      TestEntryResultCompletionCallback create_entry_callback;
      disk_cache::EntryResult result = backend_->CreateEntry(
          test_entry.url, net::HIGHEST, create_entry_callback.callback());
      result = create_entry_callback.GetResult(std::move(result));
      ASSERT_EQ(net::OK, result.net_error());
      disk_cache::Entry* entry = result.ReleaseEntry();
      ASSERT_TRUE(entry);

      auto io_buf =
          base::MakeRefCounted<net::IOBufferWithSize>(test_entry.size);
      std::fill(io_buf->data(), io_buf->data() + test_entry.size, 0);

      net::TestCompletionCallback write_data_callback;
      int rv = entry->WriteData(1, 0, io_buf.get(), test_entry.size,
                                write_data_callback.callback(), true);
      ASSERT_EQ(static_cast<int>(test_entry.size),
                write_data_callback.GetResult(rv));

      base::Time time;
      ASSERT_TRUE(base::Time::FromString(test_entry.date, &time));
      entry->SetLastUsedTimeForTest(time);
      entry->Close();
      task_environment_.RunUntilIdle();
    }
  }

  void ExpectClose(int actual, int expected) {
    int margin = std::max(256, expected / 5);
    int expected_min = expected - margin;
    int expected_max = expected + margin;
    EXPECT_LE(expected_min, actual);
    EXPECT_LE(actual, expected_max);
    EXPECT_GE(actual, 0);
  }

  int SizeBetween(int first, int last) {
    int size = 0;
    for (int pos = first; pos < last; ++pos)
      size += kCacheEntries[pos].size;
    return size;
  }

  int SizeAll() { return SizeBetween(0, std::size(kCacheEntries)); }

  static std::pair<bool, int64_t> CountBetween(NetworkContext* network_context,
                                               base::Time start_time,
                                               base::Time end_time) {
    base::RunLoop run_loop;
    std::pair<bool, int64_t> result(false, net::ERR_FAILED);
    std::unique_ptr<HttpCacheDataCounter> own_counter =
        HttpCacheDataCounter::CreateAndStart(
            network_context->url_request_context(), start_time, end_time,
            base::BindLambdaForTesting([&](HttpCacheDataCounter* counter,
                                           bool upper_bound,
                                           int64_t size_or_error) {
              EXPECT_EQ(counter, own_counter.get());
              result = std::make_pair(upper_bound, size_or_error);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  void TestCountBetween(int start_index, int end_index) {
    DCHECK_LE(0, start_index);
    DCHECK_LT(start_index, static_cast<int>(std::size(kCacheEntries)));
    DCHECK_LE(0, end_index);
    DCHECK_LT(end_index, static_cast<int>(std::size(kCacheEntries)));

    base::Time start_time;
    ASSERT_TRUE(
        base::Time::FromString(kCacheEntries[start_index].date, &start_time));

    base::Time end_time;
    ASSERT_TRUE(
        base::Time::FromString(kCacheEntries[end_index].date, &end_time));

    // The upper bound is "exclusive" but appropriximately so; make it clearly
    // exclusive.
    end_time -= base::Days(1);

    auto result = CountBetween(network_context_.get(), start_time, end_time);
    ASSERT_GE(result.second, 0);
    if (result.first) {  // upper bound
      ExpectClose(result.second, SizeAll());
    } else {
      ExpectClose(result.second, SizeBetween(start_index, end_index));
    }
  }

 protected:
  void InitNetworkContext() {
    mojom::NetworkContextParamsPtr context_params = CreateContextParams();
    context_params->http_cache_enabled = true;
    context_params->file_paths->http_cache_directory = cache_dir_.GetPath();

    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir cache_dir_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;

  // Stores the mojo::Remote<mojom::NetworkContext> of the most recently created
  // NetworkContext.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  raw_ptr<disk_cache::Backend> backend_ = nullptr;
};

TEST_F(HttpCacheDataCounterTest, Basic) {
  auto result =
      CountBetween(network_context_.get(), base::Time(), base::Time::Max());
  EXPECT_EQ(false, result.first);
  ExpectClose(result.second, SizeAll());

  // Backwards interval to make sure we don't hit DCHECKs.
  TestCountBetween(1, 0);

  // Technically backwards, too.
  TestCountBetween(0, 0);

  // Empty interval.
  TestCountBetween(0, 1);

  // Some non-empty ones.
  TestCountBetween(0, 3);
  TestCountBetween(2, 5);
}

// Return the sensible thing (0 bytes used) when there is no cache.
TEST(HttpCacheDataCounterTestNoCache, BeSensible) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<NetworkService> network_service(
      NetworkService::CreateForTesting());
  std::unique_ptr<NetworkContext> network_context;
  mojo::Remote<mojom::NetworkContext> network_context_remote;

  mojom::NetworkContextParamsPtr context_params = CreateContextParams();
  context_params->http_cache_enabled = false;

  network_context = std::make_unique<NetworkContext>(
      network_service.get(),
      network_context_remote.BindNewPipeAndPassReceiver(),
      std::move(context_params));
  auto result = HttpCacheDataCounterTest::CountBetween(
      network_context.get(), base::Time(), base::Time::Max());
  EXPECT_EQ(false, result.first);
  EXPECT_EQ(0, result.second);
}

}  // namespace

}  // namespace network
