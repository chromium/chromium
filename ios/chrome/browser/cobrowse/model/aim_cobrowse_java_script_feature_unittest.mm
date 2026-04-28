// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"

#import "base/base64.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

class AimCobrowseJavaScriptFeatureTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web_state_.SetBrowserState(&browser_state_);
    auto manager = std::make_unique<web::FakeWebFramesManager>();
    manager_ = manager.get();
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(manager));
    web::test::OverrideJavaScriptFeatures(
        &browser_state_, {AimCobrowseJavaScriptFeature::GetInstance()});
  }

  web::FakeWebFramesManager* manager() { return manager_; }

  web::FakeBrowserState browser_state_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> manager_;
};

// Tests that PostMessage correctly serializes the message and calls the
// JavaScript function on the main frame.
TEST_F(AimCobrowseJavaScriptFeatureTest, PostMessage) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->PostMessage(&web_state_,
                                                           message);

  std::u16string last_call = main_frame_ptr->GetLastJavaScriptCall();

  // Verify that the call was made to aimCobrowse.postMessage.
  EXPECT_TRUE(
      last_call.find(
          u"__gCrWeb.callFunctionInGcrWeb('aimCobrowse', 'postMessage',") !=
      std::u16string::npos);
}

// Tests that PostMessage handles the case where there is no main frame
// gracefully without crashing.
TEST_F(AimCobrowseJavaScriptFeatureTest, PostMessageNoMainFrame) {
  // Do not add a web frame to the manager.

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  // Should not crash if there is no main frame.
  AimCobrowseJavaScriptFeature::GetInstance()->PostMessage(&web_state_,
                                                           message);
}

// Tests that PostMessage does not call JavaScript if the main frame has no
// allowed origin.
TEST_F(AimCobrowseJavaScriptFeatureTest, PostMessageNoOrigin) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->PostMessage(&web_state_,
                                                           message);

  EXPECT_TRUE(main_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that PostMessage does not call JavaScript if the main frame has an
// origin not in the origin filter.
TEST_F(AimCobrowseJavaScriptFeatureTest, PostMessageDisallowedOrigin) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://example.com")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->PostMessage(&web_state_,
                                                           message);

  EXPECT_TRUE(main_frame_ptr->GetJavaScriptCallHistory().empty());
}
