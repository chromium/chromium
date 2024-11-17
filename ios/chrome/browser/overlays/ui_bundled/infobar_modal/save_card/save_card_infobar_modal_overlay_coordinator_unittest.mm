// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_coordinator.h"

#import "base/uuid.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/mock_overlay_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SaveCardInfobarModalOverlayCoordinatorTest : public PlatformTest {
 public:
  SaveCardInfobarModalOverlayCoordinatorTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())),
        root_view_controller_([[UIViewController alloc] init]) {
    autofill::CreditCard credit_card(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        "https://www.example.com/");
    std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile> delegate =
        MockAutofillSaveCardInfoBarDelegateMobileFactory::
            CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
                /*upload=*/true, credit_card);
    save_card_infobar_delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSaveCard,
                                            std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kModal);
    coordinator_ = [[SaveCardInfobarModalOverlayCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                           request:request_.get()
                          delegate:&delegate_];
  }

  void CreateMockSaveCardInfobarModalOverlayMediatorStubbed() {
    modalMediator_ = OCMClassMock([SaveCardInfobarModalOverlayMediator class]);
    OCMStub([modalMediator_ alloc]).andReturn(modalMediator_);
    OCMStub([modalMediator_ initWithRequest:request_.get()])
        .andReturn(modalMediator_);
  }

  ~SaveCardInfobarModalOverlayCoordinatorTest() override {
    [modalMediator_ stopMocking];
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  MockOverlayRequestCoordinatorDelegate delegate_;
  raw_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
      save_card_infobar_delegate_ = nil;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  UIViewController* root_view_controller_ = nil;
  id modalMediator_ = nil;
  SaveCardInfobarModalOverlayCoordinator* coordinator_ = nil;
};

TEST_F(SaveCardInfobarModalOverlayCoordinatorTest, resetModal) {
  CreateMockSaveCardInfobarModalOverlayMediatorStubbed();
  [coordinator_ startAnimated:NO];
  EXPECT_OCMOCK_VERIFY(modalMediator_);

  // Expect that the mediator's dismissOverlay is called when coordinator is
  // reset.
  OCMExpect([modalMediator_ dismissOverlay]);
  [coordinator_ stopAnimated:NO];
}
