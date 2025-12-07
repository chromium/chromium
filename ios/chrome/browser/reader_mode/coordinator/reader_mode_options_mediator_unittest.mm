// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "base/test/task_environment.h"
#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ReaderModeOptionsMediatorTest : public PlatformTest {
 public:
  ReaderModeOptionsMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    DistillerService* distiller_service =
        DistillerServiceFactory::GetForProfile(profile_.get());
    mediator_ = [[ReaderModeOptionsMediator alloc]
        initWithDistilledPagePrefs:distiller_service->GetDistilledPagePrefs()
                      webStateList:browser_->GetWebStateList()];
  }

  void TearDown() override { [mediator_ disconnect]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ReaderModeOptionsMediator* mediator_;
};

// Tests that calling `disconnect` twice does not crash.
TEST_F(ReaderModeOptionsMediatorTest, DisconnectTwice) {
  [mediator_ disconnect];
  // No-op. Should not crash.
  [mediator_ disconnect];
}
