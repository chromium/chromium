// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_security_interstitial_java_script_feature.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace security_interstitials {

namespace {

// An `IOSSecurityInterstitialPage` that records the last command it receives.
class CommandReceivingInterstitialPage : public IOSSecurityInterstitialPage {
 public:
  explicit CommandReceivingInterstitialPage(web::WebState* web_state)
      : IOSSecurityInterstitialPage(web_state,
                                    GURL(),
                                    /*client=*/nullptr) {}

  std::optional<SecurityInterstitialCommand> last_command() const {
    return last_command_;
  }

 private:
  void HandleCommand(SecurityInterstitialCommand command) override {
    last_command_ = command;
  }
  bool ShouldCreateNewNavigation() const override { return false; }
  void PopulateInterstitialStrings(
      base::DictValue& load_time_data) const override {}

  std::optional<SecurityInterstitialCommand> last_command_;
};

}  // namespace

class IOSSecurityInterstitialJavaScriptFeatureTest : public PlatformTest {
 protected:
  IOSSecurityInterstitialJavaScriptFeatureTest()
      : feature_(IOSSecurityInterstitialJavaScriptFeature::GetInstance()) {
    IOSBlockingPageTabHelper::CreateForWebState(&web_state_);

    // Commit a navigation and associate a blocking page with it so that
    // commands are routed to the page.
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    web_state_.OnNavigationFinished(&context);
    auto page = std::make_unique<CommandReceivingInterstitialPage>(&web_state_);
    page_ = page.get();
    IOSBlockingPageTabHelper::FromWebState(&web_state_)
        ->AssociateBlockingPage(context.GetNavigationId(), std::move(page));
  }

  // Sends a script message containing `command` from the main frame.
  void SendCommand(
      SecurityInterstitialCommand command,
      bool is_user_interacting,
      std::optional<GURL> request_url = GURL("file:///error_page.html")) {
    base::Value body(base::DictValue().Set(
        "command", base::NumberToString(static_cast<int>(command))));
    web::ScriptMessage message(
        std::make_unique<base::Value>(std::move(body)), is_user_interacting,
        /*is_main_frame=*/true, request_url, url::Origin());
    feature_->ScriptMessageReceived(&web_state_, message);
  }

  web::FakeWebState web_state_;
  raw_ptr<IOSSecurityInterstitialJavaScriptFeature> feature_ = nullptr;
  raw_ptr<CommandReceivingInterstitialPage> page_ = nullptr;
};

// Test that commands sent while the user is interacting with the page are
// dispatched to the blocking page.
TEST_F(IOSSecurityInterstitialJavaScriptFeatureTest,
       DispatchesCommandWithUserInteraction) {
  SendCommand(CMD_PROCEED, /*is_user_interacting=*/true);
  EXPECT_EQ(CMD_PROCEED, page_->last_command());
}

// Test that commands sent without a user interaction are ignored.
TEST_F(IOSSecurityInterstitialJavaScriptFeatureTest,
       IgnoresCommandWithoutUserInteraction) {
  SendCommand(CMD_PROCEED, /*is_user_interacting=*/false);
  EXPECT_FALSE(page_->last_command().has_value());
}

// Test that commands sent from non-file URLs are ignored even with user
// interaction.
TEST_F(IOSSecurityInterstitialJavaScriptFeatureTest,
       IgnoresCommandFromNonFileUrl) {
  SendCommand(CMD_PROCEED, /*is_user_interacting=*/true,
              GURL("https://example.test/"));
  EXPECT_FALSE(page_->last_command().has_value());
}

}  // namespace security_interstitials
