// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service.h"

#include <memory>

#include "base/macros.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class PrerenderServiceTest : public PlatformTest {
 public:
  PrerenderServiceTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    service_ = std::make_unique<PrerenderService>(browser_state_.get());
  }
  ~PrerenderServiceTest() override = default;

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<PrerenderService> service_;
  web::TestWebState web_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrerenderServiceTest);
};

}  // namespace

TEST_F(PrerenderServiceTest, NoPrerender) {
  GURL test_url("https://www.google.com");
  EXPECT_FALSE(service_->HasPrerenderForUrl(test_url));

  EXPECT_FALSE(service_->IsWebStatePrerendered(&web_state_));
}
