// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"

#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::FormActivityParams;

namespace {

FormActivityParams CreateFormActivityParams(const std::string field_type) {
  FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = field_type;
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  return params;
}

}  // namespace

class FormInputAccessoryMediatorTest : public PlatformTest {
 protected:
  FormInputAccessoryMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()),
        web_state_list_(&web_state_list_delegate_),
        test_form_activity_tab_helper_(test_web_state_.get()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    GURL url("http://foo.com");
    test_web_state_->SetCurrentURL(url);

    web::ContentWorld content_world =
        ContentWorldForAutofillJavascriptFeatures();
    test_web_state_->SetWebFramesManager(
        content_world, std::make_unique<web::FakeWebFramesManager>());
    main_frame_ = web::FakeWebFrame::CreateMainWebFrame(url);

    test_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    AutofillBottomSheetTabHelper::CreateForWebState(test_web_state_.get());

    web_state_list_.InsertWebState(
        std::move(test_web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

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
                                      reauthenticationModule:nil
                                           engagementTracker:nil];
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
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
  FormActivityParams params =
      CreateFormActivityParams(/*field_type=*/"select-one");

  OCMExpect([handler_ resetFormInputView]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  [handler_ verify];
}

// Tests consumer and handler are not reset when a field is text.
TEST_F(FormInputAccessoryMediatorTest, TextDoesNotReset) {
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  [[handler_ reject] resetFormInputView];
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
}

// Tests that suggestions are updated and shown.
TEST_F(FormInputAccessoryMediatorTest, ShowSuggestions) {
  id providerMock = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectProvider:providerMock];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  __block FormSuggestionsReadyCompletion suggestionsQueryCompletion;

  OCMStub([providerMock
              retrieveSuggestionsForForm:params
                                webState:web_state_list_.GetActiveWebState()
                accessoryViewUpdateBlock:OCMOCK_ANY])
      .andDo(^(NSInvocation* invocation) {
        __weak FormSuggestionsReadyCompletion completion;
        [invocation getArgument:&completion atIndex:4];
        suggestionsQueryCompletion = [completion copy];
      });

  // Emit a form registration event to trigger the show suggestions code path.
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"value"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  NSArray<FormSuggestion*>* suggestions = [NSArray arrayWithObject:suggestion];

  // Expect to update the view model with the suggestions from the query.
  OCMExpect([consumer_ showAccessorySuggestions:suggestions]);

  // Run the completion block to trigger the code path that updates suggestion
  // in the view model.
  suggestionsQueryCompletion(suggestions, providerMock);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that only the suggestions from the latest query in concurrent queries
// are updated and shown.
TEST_F(FormInputAccessoryMediatorTest, ShowSuggestions_WithConcurrentQueries) {
  id providerMock = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectProvider:providerMock];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  __block NSMutableArray<FormSuggestionsReadyCompletion>*
      suggestionsCompletionsQueue = [NSMutableArray array];

  OCMStub([providerMock
              retrieveSuggestionsForForm:params
                                webState:web_state_list_.GetActiveWebState()
                accessoryViewUpdateBlock:OCMOCK_ANY])
      .andDo(^(NSInvocation* invocation) {
        __weak FormSuggestionsReadyCompletion completion;
        [invocation getArgument:&completion atIndex:4];
        [suggestionsCompletionsQueue addObject:[completion copy]];
      });

  // Emit a form registration event to trigger the suggestions update code path.
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  // Emit another form registration event immediatly to trigger a concurrent
  // suggestions query.
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  ASSERT_EQ([suggestionsCompletionsQueue count], 2ul);

  FormSuggestion* suggestion1 = [FormSuggestion
      suggestionWithValue:@"value"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  NSArray<FormSuggestion*>* suggestions_from_first_query =
      [NSArray arrayWithObject:suggestion1];

  FormSuggestion* suggestion2 = [FormSuggestion
      suggestionWithValue:@"value2"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  NSArray<FormSuggestion*>* suggestions_from_second_query =
      [NSArray arrayWithObject:suggestion2];

  // Expect to update the view model only with the suggestions from the latest
  // query of the 2 concurrent queries.
  OCMReject([consumer_ showAccessorySuggestions:suggestions_from_first_query]);
  OCMExpect([consumer_ showAccessorySuggestions:suggestions_from_second_query]);

  // Run the completion block to trigger the code path that updates suggestions
  // in the view model.
  [suggestionsCompletionsQueue
   objectAtIndex:0](suggestions_from_first_query, providerMock);
  [suggestionsCompletionsQueue
   objectAtIndex:1](suggestions_from_second_query, providerMock);

  EXPECT_OCMOCK_VERIFY(consumer_);
}
