// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_contents/web_app_url_loader.h"

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "content/public/common/url_constants.h"
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
    web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          if (error_code) {
            web_contents_tester().TestDidFailLoadWithError(actual,
                                                           error_code.value());
          } else {
            web_contents_tester().TestDidFinishLoad(actual);
          }
        }));
    return result.Get();
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
  web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { web_contents_tester().TestDidFinishLoad(url2); }));
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
  web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { web_contents_tester().TestDidFinishLoad(url2); }));
  ASSERT_TRUE(result.Wait());

  // Expect successful navigation to url2.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result.Get());
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
  web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
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
