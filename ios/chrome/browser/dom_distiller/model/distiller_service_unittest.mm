// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"

#import <memory>

#import "base/barrier_callback.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "components/dom_distiller/core/article_distillation_update.h"
#import "components/dom_distiller/core/distiller_page.h"
#import "components/dom_distiller/core/dom_distiller_constants.h"
#import "components/dom_distiller/core/proto/distilled_article.pb.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Returns a once callback that captures its argument.
template <typename T>
base::OnceCallback<void(T)> CaptureArgument(T* output) {
  // SAFETY: the caller is responsible for ensuring that `output` outlives
  // the callback.
  return base::BindOnce([](T* output, T value) { *output = std::move(value); },
                        base::Unretained(output));
}

class TestDistillerPage : public dom_distiller::DistillerPage {
 public:
  void DistillPageImpl(const GURL& url, const std::string& script) override {
    base::Value empty_result;
    OnDistillationDone(url, &empty_result);
  }
  bool ShouldFetchOfflineData() override { return false; }
  dom_distiller::DistillerType GetDistillerType() override {
    return dom_distiller::DistillerType::kDOMDistiller;
  }
};

}  //  namespace

class DistillerServiceTest : public PlatformTest {
 public:
  // For readability.
  using Article = std::unique_ptr<dom_distiller::DistilledArticleProto>;

  DistillerServiceTest() : profile_(TestProfileIOS::Builder().Build()) {
    distiller_service_ = DistillerServiceFactory::GetForProfile(profile_.get());
  }

  // Requests distillation of all `urls` and returns the distillation results.
  std::vector<Article> DistillPages(const std::vector<GURL>& urls) {
    base::RunLoop run_loop;
    std::vector<Article> results;
    auto barrier = base::BarrierCallback<Article>(
        urls.size(), CaptureArgument(&results).Then(run_loop.QuitClosure()));

    // Concurrently requests all the distillations.
    for (const GURL& url : urls) {
      distiller_service_->DistillPage(url,
                                      std::make_unique<TestDistillerPage>(),
                                      barrier, base::DoNothing());
    }

    // Wait for all requests to complete.
    run_loop.Run();
    return results;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<DistillerService> distiller_service_;
};

// Test that checks that distillation completes for the same URL.
TEST_F(DistillerServiceTest, DistillSameUrl) {
  auto results = DistillPages({
      GURL("https://test.url/"),
      GURL("https://test.url/"),
  });

  EXPECT_EQ(results.size(), 2u);
  for (const auto& result : results) {
    EXPECT_TRUE(result.get() != nullptr);
  }
}

// Test that checks that distillation completes for the different URLs.
TEST_F(DistillerServiceTest, DistillDifferentUrl) {
  auto results = DistillPages({
      GURL("https://test.url/"),
      GURL("https://test2.url/"),
  });

  EXPECT_EQ(results.size(), 2u);
  for (const auto& result : results) {
    EXPECT_TRUE(result.get() != nullptr);
  }
}
