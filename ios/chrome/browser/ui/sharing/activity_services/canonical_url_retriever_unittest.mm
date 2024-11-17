// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/ui_metrics/canonical_url_share_metrics_types.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Test fixture for the retrieving canonical URLs.
class CanonicalURLRetrieverTest : public PlatformTest {
 public:
  CanonicalURLRetrieverTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  CanonicalURLRetrieverTest(const CanonicalURLRetrieverTest&) = delete;
  CanonicalURLRetrieverTest& operator=(const CanonicalURLRetrieverTest&) =
      delete;

  ~CanonicalURLRetrieverTest() override = default;

 protected:
  // Retrieves the canonical URL.
  GURL RetrieveCanonicalUrl() {
    GURL url;
    base::RunLoop run_loop;
    // Safety: The current sequence will wait for the callback to be invoked
    // before returning, thus the pointer to `url` will outlive the callback.
    activity_services::RetrieveCanonicalUrl(
        web_state(), base::BindOnce(
                         [](GURL* result, base::OnceClosure quit_closure,
                            const GURL& canonical_url) {
                           *result = canonical_url;
                           std::move(quit_closure).Run();
                         },
                         base::Unretained(&url), run_loop.QuitClosure()));
    run_loop.Run();
    return url;
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;
};

// Validates that if the canonical URL is different from the visible URL, it is
// found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLDifferentFromVisible) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("https://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE, 1);
}

// Validates that if the canonical URL is the same as the visible URL, it is
// found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLSameAsVisible) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("https://chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE, 1);
}

// Validates that if there is no canonical URL, an empty GURL is given to the
// completion block.
TEST_F(CanonicalURLRetrieverTest, TestNoCanonicalURLFound) {
  web::test::LoadHtml(@"No canonical link on this page.",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::FAILED_NO_CANONICAL_URL_DEFINED, 1);
}

// Validates that if the found canonical URL is invalid, an empty GURL is
// given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestInvalidCanonicalFound) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"chromium\">",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_CANONICAL_URL_INVALID,
                                       1);
}

// Validates that if multiple canonical URLs are found, the first one is given
// to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestMultipleCanonicalURLsFound) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">"
      @"<link rel=\"canonical\" href=\"https://chromium1.test\">",
      GURL("https://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_EQ("https://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE, 1);
}

// Validates that if the visible and canonical URLs are http, an empty GURL is
// given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLHTTP) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"http://chromium.test\">",
                      GURL("http://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_VISIBLE_URL_NOT_HTTPS,
                                       1);
}

// Validates that if the visible URL is HTTP but the canonical URL is HTTPS, an
// empty GURL is given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalURLHTTPSUpgrade) {
  web::test::LoadHtml(
      @"<link rel=\"canonical\" href=\"https://chromium.test\">",
      GURL("http://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_TRUE(url.is_empty());
  histogram_tester_.ExpectUniqueSample(ui_metrics::kCanonicalURLResultHistogram,
                                       ui_metrics::FAILED_VISIBLE_URL_NOT_HTTPS,
                                       1);
}

// Validates that if the visible URL is HTTPS but the canonical URL is HTTP, it
// is found and given to the completion block.
TEST_F(CanonicalURLRetrieverTest, TestCanonicalLinkHTTPSDowngrade) {
  web::test::LoadHtml(@"<link rel=\"canonical\" href=\"http://chromium.test\">",
                      GURL("https://m.chromium.test/"), web_state());

  GURL url = RetrieveCanonicalUrl();
  EXPECT_EQ("http://chromium.test/", url);
  histogram_tester_.ExpectUniqueSample(
      ui_metrics::kCanonicalURLResultHistogram,
      ui_metrics::SUCCESS_CANONICAL_URL_NOT_HTTPS, 1);
}
