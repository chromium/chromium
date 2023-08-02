// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_requester.h"

#import <string>

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

class FakeResultDelegate
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  FakeResultDelegate() : weak_ptr_factory_(this) {}

  FakeResultDelegate(const FakeResultDelegate&) = delete;
  FakeResultDelegate& operator=(const FakeResultDelegate&) = delete;

  ~FakeResultDelegate() override {}

  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& /* full_card_request */,
      const autofill::CreditCard& card,
      const std::u16string& cvc) override {}

  void OnFullCardRequestFailed(
      autofill::CreditCard::RecordType /* card_type */,
      autofill::payments::FullCardRequest::FailureType /* failure_type */)
      override {}

  base::WeakPtr<FakeResultDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeResultDelegate> weak_ptr_factory_;
};

class PaymentRequestFullCardRequesterTest : public PlatformTest {
 protected:
  PaymentRequestFullCardRequesterTest() {}

  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();

    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());
    personal_data_manager_.SetPrefService(chrome_browser_state_->GetPrefs());

    AddCreditCard(autofill::test::GetMaskedServerCard());  // Mastercard.

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
        /*security_origin=*/GURL());
    frames_manager->AddWebFrame(std::move(main_frame));
    autofill::AutofillJavaScriptFeature* feature =
        autofill::AutofillJavaScriptFeature::GetInstance();
    web::ContentWorld content_world = feature->GetSupportedContentWorld();
    web_state()->SetWebFramesManager(content_world, std::move(frames_manager));

    UniqueIDDataTabHelper::CreateForWebState(web_state());

    autofill_agent_ =
        [[AutofillAgent alloc] initWithPrefService:browser_state()->GetPrefs()
                                          webState:web_state()];

    InfoBarManagerImpl::CreateForWebState(web_state());
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(web_state());
    autofill_client_.reset(new autofill::ChromeAutofillClientIOS(
        browser_state(), web_state(), infobar_manager, autofill_agent_,
        /*password_generation_manager=*/nullptr));

    std::string locale("en");
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state(), autofill_client_.get(), nil, locale);
  }

  void TearDown() override {
    // Remove the frame in order to destroy the AutofillDriver before the
    // AutofillClient.
    autofill::AutofillJavaScriptFeature* feature =
        autofill::AutofillJavaScriptFeature::GetInstance();
    web::FakeWebFramesManager* frames_manager =
        static_cast<web::FakeWebFramesManager*>(
            feature->GetWebFramesManager(web_state()));
    std::string frame_id = frames_manager->GetMainWebFrame()->GetFrameId();
    frames_manager->RemoveWebFrame(frame_id);

    personal_data_manager_.SetPrefService(nullptr);
    PlatformTest::TearDown();
  }

  void AddCreditCard(const autofill::CreditCard& card) {
    personal_data_manager_.AddCreditCard(card);
  }

  web::FakeWebState* web_state() { return &web_state_; }

  TestChromeBrowserState* browser_state() {
    return chrome_browser_state_.get();
  }

  std::vector<autofill::CreditCard*> credit_cards() const {
    return personal_data_manager_.GetCreditCards();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
  autofill::TestPersonalDataManager personal_data_manager_;

  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;
  AutofillAgent* autofill_agent_;
};

// Tests that the FullCardRequester presents and dismisses the new card unmask
// prompt, when the new prompt feature flag is enabled, the full card is
// requested and when the user enters the CVC/expiration information
// respectively.
TEST_F(PaymentRequestFullCardRequesterTest, PresentAndDismissNewPrompt) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  FullCardRequester full_card_requester(base_view_controller, browser_state());

  EXPECT_EQ(nil, base_view_controller.presentedViewController);
  autofill::AutofillJavaScriptFeature* feature =
      autofill::AutofillJavaScriptFeature::GetInstance();
  web::WebFrame* main_frame =
      feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  autofill::BrowserAutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state(),
                                                           main_frame)
          ->autofill_manager();
  FakeResultDelegate* fake_result_delegate = new FakeResultDelegate;
  full_card_requester.GetFullCard(*credit_cards()[0], autofill_manager,
                                  fake_result_delegate->GetWeakPtr());

  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::Seconds(1.0));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[UINavigationController class]]);
  UINavigationController* navigation_controller =
      base::mac::ObjCCast<UINavigationController>(
          base_view_controller.presentedViewController);

  EXPECT_TRUE([navigation_controller.topViewController
      isMemberOfClass:[CardUnmaskPromptViewController class]]);

  full_card_requester.OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult::kSuccess);

  // Wait until the view controller is ordered to be dismissed and the animation
  // completes.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(10), true, ^bool {
        return !base_view_controller.presentedViewController;
      }));
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}
