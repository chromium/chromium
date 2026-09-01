// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/device_trust_challenge_tab_helper.h"

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/device_trust/device_trust_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

const char kExampleUrl[] = "https://example.com";

class DeviceTrustChallengeTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();
    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {DeviceTrustJavaScriptFeature::GetInstance()});

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());

    // FakeWebState does not provide a FakeWebFramesManager by default.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                    std::move(frames_manager));

    DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get());
  }

  DeviceTrustChallengeTabHelper* helper() {
    return DeviceTrustChallengeTabHelper::FromWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_ = nullptr;
};

// Verifies that the tab helper is created and attached to the WebState.
TEST_F(DeviceTrustChallengeTabHelperTest, CreatesSuccessfully) {
  EXPECT_NE(helper(), nullptr);
}

// Verifies that when a main web frame becomes available, the Device Trust API
// setup script is executed on that frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetUpAPIForMainFrame) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  main_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* main_frame_ptr = main_frame.get();

  web_frames_manager_->AddWebFrame(std::move(main_frame));

  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame_ptr->GetLastJavaScriptCall(),
            u"__gCrWeb.callFunctionInGcrWeb('deviceTrust', "
            u"'setupDeviceTrustAPI', []);");
}

// Verifies that non-main (child) frames do not trigger the API setup.
TEST_F(DeviceTrustChallengeTabHelperTest, IgnoreChildFrame) {
  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(GURL(kExampleUrl));
  child_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* child_frame_ptr = child_frame.get();

  web_frames_manager_->AddWebFrame(std::move(child_frame));

  EXPECT_EQ(child_frame_ptr->GetJavaScriptCallHistory().size(), 0u);
}

// Verifies that removing the current main frame does not prevent the API from
// being set up in a replacement main frame.
TEST_F(DeviceTrustChallengeTabHelperTest, SetupAPIAfterMainFrameRemoved) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  main_frame->set_browser_state(profile_.get());
  const std::string frame_id = main_frame->GetFrameId();

  web_frames_manager_->AddWebFrame(std::move(main_frame));
  web_frames_manager_->RemoveWebFrame(frame_id);

  auto replacement_frame =
      web::FakeWebFrame::CreateMainWebFrame(GURL(kExampleUrl));
  replacement_frame->set_browser_state(profile_.get());
  web::FakeWebFrame* replacement_frame_ptr = replacement_frame.get();

  web_frames_manager_->AddWebFrame(std::move(replacement_frame));

  EXPECT_EQ(replacement_frame_ptr->GetJavaScriptCallHistory().size(), 1u);
}

// Destroying the WebState invokes WebStateDestroyed() on the helper, which
// must detach its WebFramesManager observation without crashing.
TEST_F(DeviceTrustChallengeTabHelperTest, DestroyWebStateDoesNotCrash) {
  web_frames_manager_ = nullptr;
  web_state_.reset();
}

}  // namespace
