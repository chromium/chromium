// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_service_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/pre_prefetch_service.h"
#include "content/public/browser/prefetch_priority.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class PrePrefetchServiceImplTest : public testing::Test {
 public:
  PrePrefetchServiceImplTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPrefetchOffTheMainThread);
    PrePrefetchServiceImpl::SetURLLoaderFactoryForTesting(
        test_shared_url_loader_factory_.get());
  }

  void TearDown() override {
    PrePrefetchServiceImpl::SetURLLoaderFactoryForTesting(nullptr);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  TestBrowserContext* browser_context() { return &browser_context_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

// Test that `PrePrefetchServiceImpl` can be created on the UI thread.
TEST_F(PrePrefetchServiceImplTest, CreateOnUIThread) {
  auto service = PrePrefetchService::Create(browser_context());
  EXPECT_NE(service, nullptr);
}

// Test that `PrePrefetchServiceImpl::StartPrePrefetchRequest` can be called
// from non UI thread.
TEST_F(PrePrefetchServiceImplTest, StartPrePrefetchRequestFromNonUIThread) {
  const GURL prefetch_url("https://example.com/prefetch");
  base::test::TestFuture<network::ResourceRequest> request_future;

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_future.SetValue(request);
      }));

  auto service = PrePrefetchService::Create(
      browser_context(),
      /*embedder_non_ui_thread_update_headers_callbacks=*/{},
      url::Origin::Create(prefetch_url),
      /*initial_javascript_enabled_hint=*/true,
      /*initial_should_append_variations_header_hint=*/false);
  ASSERT_NE(service, nullptr);

  base::test::TestFuture<std::unique_ptr<PrePrefetchHandle>> handle_future;

  // Start PrePrefetch from non UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](PrePrefetchService* service_ptr, const GURL& url) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            return service_ptr->StartPrePrefetchRequest(
                url, test::kPreloadingEmbedderHistgramSuffixForTesting,
                /*javascript_enabled=*/true,
                /*no_vary_search_hint=*/std::nullopt,
                /*priority=*/content::PrefetchPriority::kHighest,
                /*additional_headers=*/{},
                /*request_status_listener=*/nullptr, base::TimeDelta(),
                /*should_append_variations_header=*/false,
                /*should_disable_block_until_head_timeout=*/false,
                /*should_bypass_http_cache=*/false);
          },
          service.get(), prefetch_url),
      handle_future.GetCallback());

  std::unique_ptr<PrePrefetchHandle> handle = handle_future.Take();
  EXPECT_NE(handle, nullptr);

  network::ResourceRequest request = request_future.Take();
  EXPECT_EQ(request.url, prefetch_url);
}

// Test that `PrePrefetchServiceCore::StartPrePrefetchRequest()` currently fails
// if we do not have a matched ui thread pre-calculated headers cache.
TEST_F(PrePrefetchServiceImplTest,
       StartPrePrefetchRequestFailsWithoutMatchedUIThreadHeaderCache) {
  const GURL pre_prefetch_hint_url("https://example.com/prefetch");
  const GURL pre_prefetch_actual_url("https://another.com/prefetch");

  auto service = PrePrefetchService::Create(
      browser_context(), url::Origin::Create(pre_prefetch_hint_url),
      /*initial_javascript_enabled_hint=*/true,
      /*initial_should_append_variations_header_hint=*/false);
  ASSERT_NE(service, nullptr);

  base::test::TestFuture<std::unique_ptr<PrePrefetchHandle>> handle_future;

  // Start PrePrefetch from the non UI thread, but with the origin different
  // from the hint's origin (`pre_prefetch_hint_url`'s origin vs
  // `pre_prefetch_actual_url`'s origin), meaning that the precalculated
  // headers won't match.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](PrePrefetchService* service_ptr, const GURL& url) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            return service_ptr->StartPrePrefetchRequest(
                url, test::kPreloadingEmbedderHistgramSuffixForTesting,
                /*javascript_enabled=*/true,
                /*no_vary_search_hint=*/std::nullopt,
                /*priority=*/content::PrefetchPriority::kHighest,
                /*additional_headers=*/{},
                /*request_status_listener=*/nullptr, base::TimeDelta(),
                /*should_append_variations_header=*/false,
                /*should_disable_block_until_head_timeout=*/false,
                /*should_bypass_http_cache=*/false);
          },
          service.get(), pre_prefetch_actual_url),
      handle_future.GetCallback());

  std::unique_ptr<PrePrefetchHandle> handle = handle_future.Take();
  EXPECT_EQ(handle, nullptr);
}

// Test that `PrePrefetchServiceImpl` fails if we do not have a connected
// `URLLoaderFactory`.
TEST_F(PrePrefetchServiceImplTest,
       StartPrePrefetchRequestFailsWithoutConnectedURLLoaderFactory) {
  auto local_factory = std::make_unique<network::TestURLLoaderFactory>();
  auto shared_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          local_factory.get());
  PrePrefetchServiceImpl::SetURLLoaderFactoryForTesting(shared_factory.get());

  const GURL prefetch_url("https://example.com/prefetch");
  auto service = PrePrefetchService::Create(
      browser_context(), url::Origin::Create(prefetch_url),
      /*initial_javascript_enabled_hint=*/true,
      /*initial_should_append_variations_header_hint=*/false);
  ASSERT_NE(service, nullptr);

  // Destroy the factory to close the pipe.
  local_factory.reset();

  // Wait for the mojo disconnection to be propagated to `core_` on its
  // `SequencedTaskRunner.
  RunUntilIdle();

  base::test::TestFuture<std::unique_ptr<PrePrefetchHandle>> handle_future;

  // Start PrePrefetch from non UI thread, this will match the cache, but
  // the URLLoaderFactory will disconnect.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](PrePrefetchService* service_ptr, const GURL& url) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            return service_ptr->StartPrePrefetchRequest(
                url, test::kPreloadingEmbedderHistgramSuffixForTesting,
                /*javascript_enabled=*/true,
                /*no_vary_search_hint=*/std::nullopt,
                /*priority=*/content::PrefetchPriority::kHighest,
                /*additional_headers=*/{},
                /*request_status_listener=*/nullptr, base::TimeDelta(),
                /*should_append_variations_header=*/false,
                /*should_disable_block_until_head_timeout=*/false,
                /*should_bypass_http_cache=*/false);
          },
          service.get(), prefetch_url),
      handle_future.GetCallback());

  std::unique_ptr<PrePrefetchHandle> handle = handle_future.Take();

  // PrePrefetch fails.
  EXPECT_EQ(handle, nullptr);
}

}  // namespace content
