// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service_impl.h"

#import <memory>

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

class PrerenderServiceImplTest : public PlatformTest {
 public:
  PrerenderServiceImplTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    service_ = std::make_unique<PrerenderServiceImpl>(browser_state_.get());
  }

  PrerenderServiceImplTest(const PrerenderServiceImplTest&) = delete;
  PrerenderServiceImplTest& operator=(const PrerenderServiceImplTest&) = delete;

  ~PrerenderServiceImplTest() override = default;

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<PrerenderService> service_;
  web::FakeWebState web_state_;
};

}  // namespace

TEST_F(PrerenderServiceImplTest, NoPrerender) {
  GURL test_url("https://www.google.com");
  EXPECT_FALSE(service_->HasPrerenderForUrl(test_url));

  EXPECT_FALSE(service_->IsWebStatePrerendered(&web_state_));
}
