// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"

#import <memory>

#import "components/dom_distiller/core/article_distillation_update.h"
#import "components/dom_distiller/core/distiller_page.h"
#import "components/dom_distiller/core/proto/distilled_article.pb.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

class TestDistillerPage : public dom_distiller::DistillerPage {
 public:
  void DistillPageImpl(const GURL& url, const std::string& script) override {
    base::Value empty_result;
    OnDistillationDone(url, &empty_result);
  }
  bool ShouldFetchOfflineData() override { return false; }
};

}  //  namespace

class DistillerServiceTest : public PlatformTest {
 public:
  DistillerServiceTest() : profile_(TestProfileIOS::Builder().Build()) {
    distiller_service_ = DistillerServiceFactory::GetForProfile(profile_.get());
  }

  void DistillPage(const std::string& url) {
    distiller_service_->DistillPage(
        GURL(url), std::make_unique<TestDistillerPage>(),
        base::BindOnce(&DistillerServiceTest::OnDistillArticleDone,
                       base::Unretained(this), task_environment_.QuitClosure()),
        base::BindRepeating(&DistillerServiceTest::OnDistillArticleUpdate,
                            base::Unretained(this)));
    task_environment_.RunUntilQuit();
  }

 protected:
  void OnDistillArticleDone(
      base::OnceClosure quit_closure,
      std::unique_ptr<dom_distiller::DistilledArticleProto> proto) {
    std::move(quit_closure).Run();
  }

  void OnDistillArticleUpdate(
      const dom_distiller::ArticleDistillationUpdate& article_update) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<DistillerService> distiller_service_;
};

// Test that checks that distillation completes for the same URL.
TEST_F(DistillerServiceTest, DistillSameUrl) {
  const std::string url("https://test.url/");
  DistillPage(url);
  DistillPage(url);
}

// Test that checks that distillation completes for the different URLs.
TEST_F(DistillerServiceTest, DistillDifferentUrl) {
  DistillPage("https://test.url/");
  DistillPage("https://test2.url/");
}
