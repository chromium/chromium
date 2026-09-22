// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_coordinator.h"

#import "base/functional/callback_helpers.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_mediator.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface AutofillBnplTosCoordinator (Testing)
@property(nonatomic, readonly) AutofillBnplTosMediator* mediator;
@end

class AutofillBnplTosCoordinatorTest : public PlatformTest {
 public:
  AutofillBnplTosCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;

    scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    model_.issuer.set_issuer_id(autofill::BnplIssuer::IssuerId::kBnplKlarna);

    accept_callback_called_ = false;
    cancel_callback_called_ = false;

    auto callbacks = std::make_unique<BnplCallbacks>();
    callbacks->accept_callback = base::BindOnce(
        &AutofillBnplTosCoordinatorTest::OnAccept, base::Unretained(this));
    callbacks->cancel_callback = base::BindOnce(
        &AutofillBnplTosCoordinatorTest::OnCancel, base::Unretained(this));

    coordinator_ = [[AutofillBnplTosCoordinator alloc]
             initWithModel:std::make_unique<autofill::payments::BnplTosModel>(
                               model_)
                 callbacks:std::move(callbacks)
        baseViewController:window_.rootViewController
                   browser:browser_.get()];
  }

  ~AutofillBnplTosCoordinatorTest() override {
    EXPECT_OCMOCK_VERIFY((id)scene_handler_);
  }

  void OnAccept() { accept_callback_called_ = true; }
  void OnCancel() { cancel_callback_called_ = true; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIWindow* window_;
  id<SceneCommands> scene_handler_;
  autofill::payments::BnplTosModel model_;
  bool accept_callback_called_;
  bool cancel_callback_called_;
  AutofillBnplTosCoordinator* coordinator_;
};

TEST_F(AutofillBnplTosCoordinatorTest, StartAndStop) {
  [coordinator_ start];
  EXPECT_TRUE([window_.rootViewController.presentedViewController
      isKindOfClass:[UIViewController class]]);

  [coordinator_ stop];
}

TEST_F(AutofillBnplTosCoordinatorTest, OpensNewTabForLinkClicked) {
  [coordinator_ start];
  NSURL* url = [NSURL URLWithString:@"https://example.com/privacy"];

  OCMExpect([scene_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return command.URL == GURL("https://example.com/privacy");
      }]]);

  id delegate = coordinator_;
  [delegate tosViewController:nil didTapOnURL:url];

  [coordinator_ stop];
}

// Test that tapping on a link with a non-HTTP(S) scheme or an invalid HTTP(S)
// URL does not open a new tab.
TEST_F(AutofillBnplTosCoordinatorTest,
       DoesNotOpenNewTabForNonHttpOrHttpsOrInvalidURL) {
  [coordinator_ start];
  NSURL* non_http_url = [NSURL URLWithString:@"chrome://version"];
  NSURL* invalid_http_url = [NSURL URLWithString:@"http://example.com:9999999"];

  OCMReject([scene_handler_ openURLInNewTab:[OCMArg any]]);

  id delegate = coordinator_;
  [delegate tosViewController:nil didTapOnURL:non_http_url];
  [delegate tosViewController:nil didTapOnURL:invalid_http_url];

  [coordinator_ stop];
}

TEST_F(AutofillBnplTosCoordinatorTest, TriggersAcceptCallback) {
  [coordinator_ start];

  [coordinator_.mediator didTapContinue];

  EXPECT_TRUE(accept_callback_called_);
  EXPECT_FALSE(cancel_callback_called_);

  [coordinator_ stop];
}

TEST_F(AutofillBnplTosCoordinatorTest, TriggersCancelCallback) {
  [coordinator_ start];

  [coordinator_.mediator didTapCancel];

  EXPECT_FALSE(accept_callback_called_);
  EXPECT_TRUE(cancel_callback_called_);

  [coordinator_ stop];
}
