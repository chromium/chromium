// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_contents/web_app_url_loader.h"

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {
namespace {
using base::Bucket;
using base::BucketsAre;
using ::testing::IsEmpty;

class WebAppUrlLoaderTest : public content::RenderViewHostTestHarness {
 public:
  WebAppUrlLoaderTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    loader_ = std::make_unique<WebAppUrlLoader>();
  }

  void TearDown() override {
    loader_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::WebContentsTester& web_contents_tester() {
    // RenderViewHostTestHarness always contains TestWebContents. See comments
    // in web_contents_tester.h.
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    DCHECK(web_contents_tester);
    return *web_contents_tester;
  }

  WebAppUrlLoader& loader() {
    DCHECK(loader_);
    return *loader_;
  }

  WebAppUrlLoader::Result LoadUrl(
      const GURL& desired,
      const GURL& actual,
      std::optional<int> error_code = std::nullopt) {
    base::test::TestFuture<WebAppUrlLoader::Result> result;
    loader().LoadUrl(desired, web_contents(),
                     WebAppUrlLoader::UrlComparison::kExact,
                     result.GetCallback());
    web_contents_tester().CommitPendingNavigation();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          auto simulator = content::NavigationSimulator::CreateFromPending(
              web_contents()->GetController());
          if (actual != desired) {
            simulator->Redirect(actual);
          }
          if (error_code) {
            simulator->SetKeepLoading(true);
            simulator->Commit();
            web_contents_tester().TestDidFailLoadWithError(actual,
                                                           error_code.value());
          } else {
            simulator->Commit();
          }
        }));
    return result.Get();
  }

  void FlushTaskRunner() {
    base::test::TestFuture<void> flushed;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, flushed.GetCallback());
    EXPECT_TRUE(flushed.Wait());
  }

  void CommitPrepareForLoadAndWaitForPendingTarget() {
    web_contents_tester().CommitPendingNavigation();
    FlushTaskRunner();
    ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  }

 private:
  std::unique_ptr<WebAppUrlLoader> loader_;
};

TEST_F(WebAppUrlLoaderTest, Url1Redirected_ThenUrl2Loaded) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  EXPECT_EQ(WebAppUrlLoader::Result::kRedirectedUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url2));

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

TEST_F(WebAppUrlLoaderTest, Url1DidFailLoad_ThenUrl2Loaded) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  EXPECT_EQ(WebAppUrlLoader::Result::kFailedUnknownReason,
            LoadUrl(/*desired=*/url1, /*actual=*/url1,
                    /*error_code=*/net::ERR_FAILED));

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

TEST_F(WebAppUrlLoaderTest, PrepareForLoad_ExcessiveDidFinishLoad) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  // Expect successful navigation to url1.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url2, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  // Simulate an excessive DidFinishLoad for url1 caused by active javascript
  // while in PrepareForLoad state. PrepareForLoad() acts as a barrier here:
  // it's flushing all url1-related noisy events so url2 loading will start
  // clean later.
  web_contents_tester().TestDidFinishLoad(url1);
  web_contents_tester().CommitPendingNavigation();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        web_contents_tester().CommitPendingNavigation();
      }));
  ASSERT_TRUE(result.Wait());

  // Expect successful navigation to url2.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result.Get());
}

TEST_F(WebAppUrlLoaderTest, PrepareForLoad_ExcessiveDidFailLoad) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  // Expect successful navigation to url1.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url2, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  // Simulate an excessive DidFailLoad for url1 caused by active javascript
  // while in PrepareForLoad state. PrepareForLoad() acts as a barrier here:
  // it's flushing all url1-related noisy events so url2 loading will start
  // clean later.
  web_contents_tester().TestDidFailLoadWithError(url1, /*error_code=*/1);
  web_contents_tester().CommitPendingNavigation();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        web_contents_tester().CommitPendingNavigation();
      }));
  ASSERT_TRUE(result.Wait());

  // Expect successful navigation to url2.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result.Get());
}

TEST_F(WebAppUrlLoaderTest,
       DidFinishLoadBeforeNavigationCommits_DoesNotSucceed) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url2, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());

  // Spoofed DidFinishLoad for about:blank and url2 before PrepareForLoad
  // actually commits about:blank must be ignored.
  web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
  web_contents_tester().TestDidFinishLoad(url2);
  FlushTaskRunner();
  EXPECT_FALSE(result.IsReady());

  // Commit about:blank to complete PrepareForLoad and start LoadUrlInternal.
  CommitPrepareForLoadAndWaitForPendingTarget();

  // Spoofed DidFinishLoad for url2 while navigation to url2 is still pending
  // (has not committed) must be ignored while url1/about:blank is committed.
  web_contents_tester().TestDidFinishLoad(url2);
  web_contents_tester().TestDidFinishLoad(url1);
  FlushTaskRunner();
  EXPECT_FALSE(result.IsReady());

  // Once the real navigation to url2 commits, the load succeeds.
  web_contents_tester().CommitPendingNavigation();
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result.Get());
}

TEST_F(WebAppUrlLoaderTest, NavigationAbortedWithoutCommit_Fails) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url2, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  CommitPrepareForLoadAndWaitForPendingTarget();

  // Abort the pending navigation to url2 without committing, then fire
  // DidFinishLoad for the old page and for url2.
  auto simulator = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  simulator->AbortCommit();
  web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
  web_contents_tester().TestDidFinishLoad(url1);
  web_contents_tester().TestDidFinishLoad(url2);

  EXPECT_EQ(WebAppUrlLoader::Result::kFailedUnknownReason, result.Get());
}

TEST_F(WebAppUrlLoaderTest,
       DidFinishLoadAfterDocumentChangedUrl_ReturnsRedirected) {
  const GURL url1{"https://example.com/app"};
  const GURL url2{"https://example.com/other"};

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url1, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  CommitPrepareForLoadAndWaitForPendingTarget();

  // Commit url1 while keeping the document loading, then perform a
  // renderer-initiated same-document navigation (`history.pushState`) to url2
  // before DidFinishLoad fires.
  auto simulator = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  simulator->SetKeepLoading(true);
  simulator->Commit();

  content::NavigationSimulator::CreateRendererInitiated(url2, main_rfh())
      ->CommitSameDocument();
  web_contents_tester().TestDidFinishLoad(url1);

  EXPECT_EQ(WebAppUrlLoader::Result::kRedirectedUrlLoaded, result.Get());
}

TEST_F(WebAppUrlLoaderTest, LoadUrlTimeout_FailsOnceWithTookTooLong) {
  base::HistogramTester histogram_tester;
  const GURL url{"https://example.com"};

  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  CommitPrepareForLoadAndWaitForPendingTarget();

  // Leave `url` pending and fast-forward past the load timeout. `Stop()`
  // synchronously aborts the pending navigation, which must not double-post
  // after `OnLoadUrlTimeout` already posted `kFailedPageTookTooLong`.
  task_environment()->FastForwardBy(
      WebAppUrlLoader::kSecondsToWaitForWebContentsLoad);

  EXPECT_EQ(WebAppUrlLoader::Result::kFailedPageTookTooLong, result.Get());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Webapp.WebAppUrlLoaderResult"),
      BucketsAre(Bucket(WebAppUrlLoader::Result::kFailedPageTookTooLong, 1)));
}

TEST_F(WebAppUrlLoaderTest, OverlappingLoadUrlTasksOnSameWebContents_Succeeds) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  // Start a first `LoadUrl` call that commits `url1` but stays loading
  // (`SetKeepLoading(true)`) so its `LoaderTask` remains alive on
  // `web_contents()`.
  base::test::TestFuture<WebAppUrlLoader::Result> result1;
  loader().LoadUrl(url1, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result1.GetCallback());
  CommitPrepareForLoadAndWaitForPendingTarget();
  {
    auto simulator1 = content::NavigationSimulator::CreateFromPending(
        web_contents()->GetController());
    simulator1->SetKeepLoading(true);
    simulator1->Commit();
  }
  EXPECT_FALSE(result1.IsReady());

  // Start a second `LoadUrl` call on the same `web_contents()` while the first
  // `LoaderTask` is still alive. Its `PrepareForLoad(about:blank)` and `url2`
  // navigations must not be hijacked by the older `LoaderTask` or stall waiting
  // for `kSecondsToWaitForWebContentsLoad`.
  base::test::TestFuture<WebAppUrlLoader::Result> result2;
  loader().LoadUrl(url2, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result2.GetCallback());
  CommitPrepareForLoadAndWaitForPendingTarget();
  web_contents_tester().CommitPendingNavigation();

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result2.Get());
}

TEST_F(WebAppUrlLoaderTest, RecordsHistogram) {
  base::HistogramTester histogram_tester;

  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));
  EXPECT_THAT(histogram_tester.GetAllSamples("Webapp.WebAppUrlLoaderResult"),
              BucketsAre(Bucket(WebAppUrlLoader::Result::kUrlLoaded, 1)));
  EXPECT_EQ(WebAppUrlLoader::Result::kFailedUnknownReason,
            LoadUrl(/*desired=*/url2, /*actual=*/url2,
                    /*error_code=*/net::ERR_FAILED));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Webapp.WebAppUrlLoaderResult"),
      BucketsAre(Bucket(WebAppUrlLoader::Result::kUrlLoaded, 1),
                 Bucket(WebAppUrlLoader::Result::kFailedUnknownReason, 1)));
}

TEST_F(WebAppUrlLoaderTest, RecordsPrepareForLoadHistogram) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<void> future;
  loader().PrepareForLoad(web_contents(), future.GetCallback());
  web_contents_tester().CommitPendingNavigation();
  EXPECT_TRUE(future.Wait());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Webapp.WebAppUrlLoaderPrepareForLoadResult"),
              BucketsAre(Bucket(WebAppUrlLoader::Result::kUrlLoaded, 1)));
}

TEST_F(WebAppUrlLoaderTest, RecordsHistogramOnWebContentsDestroyed) {
  base::HistogramTester histogram_tester;
  const GURL url{"https://example.com"};
  base::test::TestFuture<WebAppUrlLoader::Result> result;
  loader().LoadUrl(url, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                   result.GetCallback());
  DeleteContents();
  EXPECT_EQ(WebAppUrlLoader::Result::kFailedWebContentsDestroyed, result.Get());
  EXPECT_THAT(histogram_tester.GetAllSamples("Webapp.WebAppUrlLoaderResult"),
              BucketsAre(Bucket(
                  WebAppUrlLoader::Result::kFailedWebContentsDestroyed, 1)));
}

TEST_F(WebAppUrlLoaderTest, NoHistogramOnLoaderDestruction) {
  base::HistogramTester histogram_tester;
  const GURL url{"https://example.com"};
  auto loader = std::make_unique<WebAppUrlLoader>();
  bool callback_called = false;
  auto callback = base::BindLambdaForTesting(
      [&](WebAppUrlLoader::Result) { callback_called = true; });
  loader->LoadUrl(url, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
                  std::move(callback));
  // Destroy the loader. No metrics should be recorded.
  loader.reset();
  // Simulate the load finishing. The callback should still not be called
  // because the loader is gone.
  web_contents_tester().TestDidFinishLoad(url);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);
  EXPECT_THAT(histogram_tester.GetAllSamples("Webapp.WebAppUrlLoaderResult"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Webapp.WebAppUrlLoaderPrepareForLoadResult"),
              IsEmpty());
}

}  // namespace
}  // namespace webapps
