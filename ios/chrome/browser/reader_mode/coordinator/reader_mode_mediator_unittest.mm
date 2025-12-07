// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"

#import "base/test/task_environment.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ReaderModeMediatorTest : public PlatformTest {
 public:
  ReaderModeMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    DistillerService* distiller_service =
        DistillerServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[ReaderModeMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()
                  BWGService:BwgServiceFactory::GetForProfile(profile_.get())
          distilledPagePrefs:distiller_service->GetDistilledPagePrefs()];
  }

  void TearDown() override { [mediator_ disconnect]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ReaderModeMediator* mediator_;
};

TEST_F(ReaderModeMediatorTest, SetDefaultTheme) {
  [mediator_ setDefaultTheme:dom_distiller::mojom::Theme::kDark];
  EXPECT_EQ(mediator_.distilledPagePrefs->GetTheme(),
            dom_distiller::mojom::Theme::kDark);

  [mediator_ setDefaultTheme:dom_distiller::mojom::Theme::kLight];
  EXPECT_EQ(mediator_.distilledPagePrefs->GetTheme(),
            dom_distiller::mojom::Theme::kLight);
}
