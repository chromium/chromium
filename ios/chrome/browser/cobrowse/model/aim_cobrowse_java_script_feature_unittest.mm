// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"

#import "base/base64.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
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
    web_state_.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                   std::move(manager));
    web::test::OverrideJavaScriptFeatures(
        &browser_state_, {AimCobrowseJavaScriptFeature::GetInstance()});
  }

  void DeliverScriptMessage(web::WebState* web_state,
                            const web::ScriptMessage& message) {
    AimCobrowseJavaScriptFeature::GetInstance()->ScriptMessageReceived(
        web_state, message);
  }

  web::FakeWebFramesManager* manager() { return manager_; }

  web::WebTaskEnvironment task_environment_;
  web::FakeBrowserState browser_state_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> manager_;
};

// Tests that SendNativeToWeb correctly serializes the message and calls the
// JavaScript function on the main frame.
TEST_F(AimCobrowseJavaScriptFeatureTest, SendNativeToWeb) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://www.google.com")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(&web_state_,
                                                               message);

  std::u16string last_call = main_frame_ptr->GetLastJavaScriptCall();

  // Verify that the call was made to aimCobrowse.sendNativeToWeb.
  EXPECT_TRUE(
      last_call.find(
          u"__gCrWeb.callFunctionInGcrWeb('aimCobrowse', 'sendNativeToWeb',") !=
      std::u16string::npos);
}

// Tests that SendNativeToWeb handles the case where there is no main frame
// gracefully without crashing.
TEST_F(AimCobrowseJavaScriptFeatureTest, SendNativeToWebNoMainFrame) {
  // Do not add a web frame to the manager.

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  // Should not crash if there is no main frame.
  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(&web_state_,
                                                               message);
}

// Tests that SendNativeToWeb does not call JavaScript if the main frame has no
// allowed origin.
TEST_F(AimCobrowseJavaScriptFeatureTest, SendNativeToWebNoOrigin) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(&web_state_,
                                                               message);

  EXPECT_TRUE(main_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that SendNativeToWeb does not call JavaScript if the main frame has an
// origin not in the origin filter.
TEST_F(AimCobrowseJavaScriptFeatureTest, SendNativeToWebDisallowedOrigin) {
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
      url::Origin::Create(GURL("https://example.com")));
  auto* main_frame_ptr = main_frame.get();
  main_frame_ptr->set_browser_state(&browser_state_);
  manager()->AddWebFrame(std::move(main_frame));

  lens::ClientToAimMessage message;
  message.mutable_open_threads_view()->mutable_payload();

  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(&web_state_,
                                                               message);

  EXPECT_TRUE(main_frame_ptr->GetJavaScriptCallHistory().empty());
}

// Tests that script messages from non-main (child) frames are ignored and not
// forwarded to the tab helper.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageFromChildFrameIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  std::string serialized_message;
  handshake_response.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  // Create a script message with is_main_frame = false.
  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/false,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  // Directly deliver the script message using the fixture helper.
  DeliverScriptMessage(&web_state_, script_message);

  // Verify that the callback was NOT invoked and handshake remains false.
  EXPECT_FALSE(callback_invoked);
  EXPECT_FALSE(tab_helper->IsHandshakeReceived());
}

// Tests that script messages received on a WebState that does not have an
// AssistantAimTabHelper attached are ignored and do not cause a crash.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageNoTabHelperIgnored) {
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  std::string serialized_message;
  handshake_response.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  // Directly deliver the script message using the fixture helper.
  // This should not crash.
  DeliverScriptMessage(&web_state_, script_message);
}

// Tests that script messages with no body are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageNoBodyIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  // Create a script message with a null body.
  web::ScriptMessage script_message(
      /*body=*/nullptr,
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that script messages where the body is not a dictionary are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageBodyNotDictIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  // Create a script message where body is a simple string value.
  base::Value body("invalid_body_type");
  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that script messages missing the "message" key are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageMissingKeyIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  // Create a dictionary missing the "message" key.
  base::DictValue dict;
  dict.Set("wrong_key", base::Value("some_value"));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that script messages containing invalid base64 data are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageInvalidBase64Ignored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  // "!!!" is not a valid base64 string.
  base::DictValue dict;
  dict.Set("message", base::Value("!!!"));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that script messages containing invalid protobuf payloads are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageInvalidProtobufIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  // Correct base64 encoding of "invalid_proto_data".
  std::string base64_message = base::Base64Encode("invalid_proto_data");

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that a valid script message is successfully parsed and forwarded to the
// tab helper.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessagePropagatedSuccessfully) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  bool handshake_received = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, bool* handshake, const lens::AimToClientMessage& msg) {
        *invoked = true;
        if (msg.has_handshake_response()) {
          *handshake = true;
        }
      },
      &callback_invoked, &handshake_received));

  // Create a valid AimToClientMessage with handshake response.
  lens::AimToClientMessage message;
  message.mutable_handshake_response();
  std::string serialized_message;
  message.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_TRUE(callback_invoked);
  EXPECT_TRUE(handshake_received);
}

// Tests that script messages from invalid URLs (e.g. www.google.com without
// udm=50) are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageFromInvalidUrlIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  lens::AimToClientMessage message;
  message.mutable_handshake_response();
  std::string serialized_message;
  message.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://www.google.com"),
      /*security_origin=*/url::Origin::Create(GURL("https://www.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}

// Tests that script messages from AMP URLs are ignored.
TEST_F(AimCobrowseJavaScriptFeatureTest, ScriptMessageFromAmpUrlIgnored) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, const lens::AimToClientMessage& msg) {
        *invoked = true;
      },
      &callback_invoked));

  lens::AimToClientMessage message;
  message.mutable_handshake_response();
  std::string serialized_message;
  message.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::DictValue dict;
  dict.Set("message", base::Value(base64_message));
  base::Value body(std::move(dict));

  web::ScriptMessage script_message(
      std::make_unique<base::Value>(std::move(body)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/GURL("https://amp.google.com/search?udm=50&q=test"),
      /*security_origin=*/url::Origin::Create(GURL("https://amp.google.com")));

  DeliverScriptMessage(&web_state_, script_message);

  EXPECT_FALSE(callback_invoked);
}
