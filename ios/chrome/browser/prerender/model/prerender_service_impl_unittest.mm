// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_service_impl.h"

#import <memory>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

class PrerenderServiceImplTest : public PlatformTest {
 public:
  PrerenderServiceImplTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    service_ = std::make_unique<PrerenderServiceImpl>(profile_.get());
  }

  PrerenderServiceImplTest(const PrerenderServiceImplTest&) = delete;
  PrerenderServiceImplTest& operator=(const PrerenderServiceImplTest&) = delete;

  ~PrerenderServiceImplTest() override = default;

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<PrerenderService> service_;
  web::FakeWebState web_state_;
};

}  // namespace

TEST_F(PrerenderServiceImplTest, NoPrerender) {
  GURL test_url("https://www.google.com");
  EXPECT_FALSE(service_->HasPrerenderForUrl(test_url));

  EXPECT_FALSE(service_->IsWebStatePrerendered(&web_state_));
}
