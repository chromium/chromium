// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_container.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PrePrefetchContainerTest : public testing::Test {
 public:
  PrePrefetchContainerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPrefetchOffTheMainThread);
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    browser_context_ = std::make_unique<TestBrowserContext>();
    // Take a weak pointer while on the main thread, so that it can be passed on
    // a non-main thread later in `CreatePrefetchRequest()`.
    browser_context_weak_ = browser_context_->GetWeakPtr();
  }
  ~PrePrefetchContainerTest() override = default;

  std::unique_ptr<const PrefetchRequest> CreatePrefetchRequest(
      const GURL& url) {
    EXPECT_TRUE(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    return PrefetchRequest::
        CreateBrowserInitiatedWithoutWebContentsOffTheMainThread(
            browser_context_weak_, url,
            PrefetchType(PreloadingTriggerType::kEmbedder,
                         /*use_prefetch_proxy=*/false),
            test::kPreloadingEmbedderHistgramSuffixForTesting,
            blink::mojom::Referrer(),
            /*javascript_enabled=*/true,
            /*referring_origin=*/std::nullopt,
            /*no_vary_search_hint=*/std::nullopt,
            /*priority=*/std::nullopt);
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  GetURLLoaderFactoryRemote() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
    test_url_loader_factory_.Clone(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  base::WeakPtr<BrowserContext> browser_context_weak_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Confirms that `PrePrefetchContainer` starts and sends a network
// request when `PrePrefetchContainer::CreateAndStart(ForTesting)` is called.
TEST_F(PrePrefetchContainerTest, StartPrePrefetch) {
  const GURL prefetch_url("https://example.com/prefetch");
  auto url_loader_factory_remote = GetURLLoaderFactoryRemote();

  base::test::TestFuture<std::unique_ptr<PrePrefetchContainer>> future;
  base::RunLoop run_loop;

  test_url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [&](const network::ResourceRequest& request) { run_loop.Quit(); }));

  task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](PrePrefetchContainerTest* test_fixture, const GURL& url,
             mojo::PendingRemote<network::mojom::URLLoaderFactory> factory) {
            auto prefetch_request = test_fixture->CreatePrefetchRequest(url);
            PrefetchUpdateHeadersParams ui_thread_pre_calculated_headers;
            return PrePrefetchContainer::CreateAndStartForTesting(
                std::move(prefetch_request), std::move(factory),
                ui_thread_pre_calculated_headers,
                /*non_ui_thread_update_headers_callbacks=*/{});
          },
          base::Unretained(this), prefetch_url,
          std::move(url_loader_factory_remote)),
      future.GetCallback());

  // Wait until the request is received and the container is created.
  run_loop.Run();
  std::unique_ptr<PrePrefetchContainer> container = future.Take();

  ASSERT_TRUE(container);
  auto* pending_requests = test_url_loader_factory()->pending_requests();
  ASSERT_EQ(1u, pending_requests->size());
  EXPECT_EQ(prefetch_url, (*pending_requests)[0].request.url);
}

// Tests that `PrePrefetchContainer` with `ui_thread_pre_calculated_headers`
// (by `PrefetchUpdateHeadersParams`) sends a network request with those
// specified header params.
TEST_F(PrePrefetchContainerTest,
       StartPrePrefetchWithUIThreadPreCalculatedHeaders) {
  const GURL prefetch_url("https://example.com/prefetch");
  auto url_loader_factory_remote = GetURLLoaderFactoryRemote();

  base::test::TestFuture<std::unique_ptr<PrePrefetchContainer>> future;
  base::test::TestFuture<network::ResourceRequest> request_future;

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_future.SetValue(request);
      }));

  task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](PrePrefetchContainerTest* test_fixture, const GURL& url,
             mojo::PendingRemote<network::mojom::URLLoaderFactory> factory) {
            auto prefetch_request = test_fixture->CreatePrefetchRequest(url);
            PrefetchUpdateHeadersParams ui_thread_base_headers;
            ui_thread_base_headers.modified_headers.SetHeader("X-Test-Header",
                                                              "Value1");
            ui_thread_base_headers.modified_cors_exempt_headers.SetHeader(
                "X-Test-Cors-Exempt-Header", "Value2");

            return PrePrefetchContainer::CreateAndStartForTesting(
                std::move(prefetch_request), std::move(factory),
                std::move(ui_thread_base_headers),
                /*non_ui_thread_update_headers_callbacks=*/{});
          },
          base::Unretained(this), prefetch_url,
          std::move(url_loader_factory_remote)),
      future.GetCallback());

  // Wait until the request is received and the container is created.
  std::unique_ptr<PrePrefetchContainer> container = future.Take();
  ASSERT_TRUE(container);

  network::ResourceRequest request = request_future.Take();
  ASSERT_EQ(request.url, prefetch_url);

  // Check that the intercepted request has the expected header params.
  EXPECT_EQ(request.headers.GetHeader("X-Test-Header"),
            std::optional<std::string>("Value1"));
  EXPECT_EQ(request.cors_exempt_headers.GetHeader("X-Test-Cors-Exempt-Header"),
            std::optional<std::string>("Value2"));
}

// Tests that the `PrePrefetchUpdateHeadersCallback`s passed to
// `PrePrefetchContainer::CreateAndStart(ForTesting)` are correctly executed
// on the non-UI thread and their modifications to `ResourceRequest` headers
// are applied.
TEST_F(PrePrefetchContainerTest,
       StartPrePrefetchWithNonUIThreadUpdateHeadersCallbacks) {
  const GURL prefetch_url("https://example.com/prefetch");

  auto callback =
      base::BindLambdaForTesting([](const network::ResourceRequest& request) {
        EXPECT_TRUE(!BrowserThread::CurrentlyOn(BrowserThread::UI));
        PrefetchUpdateHeadersParams params;
        params.modified_headers.SetHeader("X-Test-Header", "Value1");
        params.modified_cors_exempt_headers.SetHeader(
            "X-Test-Cors-Exempt-Header", "Value2");
        return params;
      });

  auto url_loader_factory_remote = GetURLLoaderFactoryRemote();

  base::test::TestFuture<std::unique_ptr<PrePrefetchContainer>> future;
  base::test::TestFuture<network::ResourceRequest> request_future;

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request_future.SetValue(request);
      }));

  task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](PrePrefetchContainerTest* test_fixture, const GURL& url,
             mojo::PendingRemote<network::mojom::URLLoaderFactory> factory,
             PrePrefetchUpdateHeadersCallback callback) {
            auto prefetch_request = test_fixture->CreatePrefetchRequest(url);
            PrefetchUpdateHeadersParams ui_thread_pre_calculated_headers;

            std::vector<PrePrefetchUpdateHeadersCallback> callbacks;
            callbacks.push_back(std::move(callback));

            return PrePrefetchContainer::CreateAndStartForTesting(
                std::move(prefetch_request), std::move(factory),
                ui_thread_pre_calculated_headers, std::move(callbacks));
          },
          base::Unretained(this), prefetch_url,
          std::move(url_loader_factory_remote), std::move(callback)),
      future.GetCallback());

  // Wait until the request is received and the container is created.
  std::unique_ptr<PrePrefetchContainer> container = future.Take();
  ASSERT_TRUE(container);

  network::ResourceRequest request = request_future.Take();
  ASSERT_EQ(request.url, prefetch_url);

  // Check that the intercepted request has the expected header params.
  EXPECT_EQ(request.headers.GetHeader("X-Test-Header"),
            std::optional<std::string>("Value1"));
  EXPECT_EQ(request.cors_exempt_headers.GetHeader("X-Test-Cors-Exempt-Header"),
            std::optional<std::string>("Value2"));
}

}  // namespace content
