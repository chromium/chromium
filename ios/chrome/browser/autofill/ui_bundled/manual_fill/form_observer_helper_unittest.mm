// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_observer_helper.h"

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class ManualFillFormObserverHelperiOSTest : public PlatformTest {
 public:
  ManualFillFormObserverHelperiOSTest()
      : web_state_list_(&web_state_list_delegate_) {}

  ManualFillFormObserverHelperiOSTest(
      const ManualFillFormObserverHelperiOSTest&) = delete;
  ManualFillFormObserverHelperiOSTest& operator=(
      const ManualFillFormObserverHelperiOSTest&) = delete;

  ~ManualFillFormObserverHelperiOSTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    _helper =
        [[FormObserverHelper alloc] initWithWebStateList:&web_state_list_];
    _mockDelegate = OCMProtocolMock(@protocol(FormActivityObserver));
    _helper.delegate = _mockDelegate;
  }

  void TearDown() override {
    _helper = nil;
    _mockDelegate = nil;
    PlatformTest::TearDown();
  }

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FormObserverHelper* _helper;
  OCMockObject<FormActivityObserver>* _mockDelegate;

  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(GURL(url));
    return test_web_state;
  }

  void AppendNewWebState(const char* url) {
    web_state_list_.InsertWebState(CreateWebState(url));
  }
};

// Tests that an observer is correctly created and set up.
TEST_F(ManualFillFormObserverHelperiOSTest, Creation) {}

// Test that the observer delegates one active web state callbacks.
TEST_F(ManualFillFormObserverHelperiOSTest, ObservesWebState) {
  AppendNewWebState("https://example.com");
  web_state_list_.ActivateWebStateAt(0);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "blur";
  params.value = "value";
  params.input_missing = false;

  OCMExpect([_mockDelegate
                     webState:static_cast<web::WebState*>([OCMArg anyPointer])
      didRegisterFormActivity:params
                      inFrame:static_cast<web::WebFrame*>(
                                  [OCMArg anyPointer])]);

  autofill::TestFormActivityTabHelper test_form_activity_tab_helper(
      web_state_list_.GetActiveWebState());
  test_form_activity_tab_helper.FormActivityRegistered(nullptr, params);
  @try {
    [_mockDelegate verify];
  } @catch (NSException* exception) {
    ADD_FAILURE();
  }
}

// Test that the observer delegates the callbacks with multiple active web
// state.
TEST_F(ManualFillFormObserverHelperiOSTest, ObservesMultipleWebStates) {
  AppendNewWebState("https://example.com");
  AppendNewWebState("https://chrome.com");

  autofill::TestFormActivityTabHelper test_form_activity_tab_helper0(
      web_state_list_.GetWebStateAt(0));
  autofill::TestFormActivityTabHelper test_form_activity_tab_helper1(
      web_state_list_.GetWebStateAt(1));

  web_state_list_.ActivateWebStateAt(0);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "focus";
  params.value = "value";
  params.input_missing = false;

  @try {
    OCMExpect([_mockDelegate
                       webState:static_cast<web::WebState*>([OCMArg anyPointer])
        didRegisterFormActivity:params
                        inFrame:static_cast<web::WebFrame*>(
                                    [OCMArg anyPointer])]);

    test_form_activity_tab_helper0.FormActivityRegistered(nullptr, params);
    [_mockDelegate verify];

    web_state_list_.ActivateWebStateAt(1);

    OCMExpect([_mockDelegate
                       webState:static_cast<web::WebState*>([OCMArg anyPointer])
        didRegisterFormActivity:params
                        inFrame:static_cast<web::WebFrame*>(
                                    [OCMArg anyPointer])]);
    test_form_activity_tab_helper1.FormActivityRegistered(nullptr, params);
    [_mockDelegate verify];

    web_state_list_.ActivateWebStateAt(0);
    OCMExpect([_mockDelegate
                       webState:static_cast<web::WebState*>([OCMArg anyPointer])
        didRegisterFormActivity:params
                        inFrame:static_cast<web::WebFrame*>(
                                    [OCMArg anyPointer])]);
    test_form_activity_tab_helper0.FormActivityRegistered(nullptr, params);
    [_mockDelegate verify];
  } @catch (NSException* exception) {
    ADD_FAILURE();
  }
}
