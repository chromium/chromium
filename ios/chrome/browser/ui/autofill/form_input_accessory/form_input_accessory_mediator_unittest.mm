// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FormInputAccessoryMediatorTest : public PlatformTest {
 protected:
  FormInputAccessoryMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()),
        web_state_list_(&web_state_list_delegate_),
        test_form_activity_tab_helper_(test_web_state_.get()) {}

  void SetUp() override {
    GURL url("http://foo.com");
    test_web_state_->SetCurrentURL(url);

    web::ContentWorld content_world =
        autofill::FormUtilJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    test_web_state_->SetWebFramesManager(
        content_world, std::make_unique<web::FakeWebFramesManager>());
    main_frame_ = web::FakeWebFrame::CreateMainWebFrame(url);

    test_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state_list_.InsertWebState(0, std::move(test_web_state_),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    consumer_ = OCMProtocolMock(@protocol(FormInputAccessoryConsumer));
    handler_ = OCMProtocolMock(@protocol(FormInputAccessoryMediatorHandler));

    mediator_ =
        [[FormInputAccessoryMediator alloc] initWithConsumer:consumer_
                                                     handler:handler_
                                                webStateList:&web_state_list_
                                         personalDataManager:nullptr
                                        profilePasswordStore:nullptr
                                        accountPasswordStore:nullptr
                                        securityAlertHandler:nil
                                      reauthenticationModule:nil];
  }
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  std::unique_ptr<web::FakeWebFrame> main_frame_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  id consumer_;
  id handler_;
  autofill::TestFormActivityTabHelper test_form_activity_tab_helper_;
  FormInputAccessoryMediator* mediator_;
};

// Tests FormInputAccessoryMediator can be initialized.
TEST_F(FormInputAccessoryMediatorTest, Init) {
  EXPECT_TRUE(mediator_);
}

// Tests consumer and handler are reset when a field is a picker.
TEST_F(FormInputAccessoryMediatorTest, PickerReset) {
  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "select-one";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;

  OCMExpect([handler_ resetFormInputView]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  [handler_ verify];
}

// Tests consumer and handler are not reset when a field is text.
TEST_F(FormInputAccessoryMediatorTest, TextDoesNotReset) {
  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;

  [[handler_ reject] resetFormInputView];
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
}
