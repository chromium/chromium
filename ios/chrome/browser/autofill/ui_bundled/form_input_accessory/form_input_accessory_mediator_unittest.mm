// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"

#import "base/metrics/histogram_base.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/filling/filling_product.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

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

// Test implementation of FormSuggestionProvider.
@interface TestFormSuggestionProvider : NSObject <FormSuggestionProvider>
@end

@implementation TestFormSuggestionProvider

#pragma mark - Public

// Sets the SuggestionProviderType returned when using the -type getter.
- (void)setType:(SuggestionProviderType)type {
  _type = type;
}

// Sets the FillingProduct returned when using the -mainFillingProduct getter.
- (void)setMainFillingProduct:(autofill::FillingProduct)mainFillingProduct {
  _mainFillingProduct = mainFillingProduct;
}

#pragma mark - FormSuggestionProvider

@synthesize type = _type;
@synthesize mainFillingProduct = _mainFillingProduct;

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(autofill::FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
}

@end

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
                                           engagementTracker:nullptr];
  }

  void TearDown() override {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(handler_);
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

// Tests consumer and handler are reset when a field is a picker on iPad.
TEST_F(FormInputAccessoryMediatorTest, PickerReset) {
  // On iPhone, a default input view with navigation buttons is shown for a
  // picker instead of resetting. Therefore, the test should be skipped if the
  // device is not an iPad.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    GTEST_SKIP() << "Skipping FormInputAccessoryMediatorTest.PickerReset: The "
                    "test is for iPad.";
  }

  FormActivityParams params =
      CreateFormActivityParams(/*field_type=*/"select-one");

  OCMExpect([handler_ resetFormInputView]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  [handler_ verify];
}

// Tests consumer is set to show navigation buttons when a field is a picker on
// iPhone
TEST_F(FormInputAccessoryMediatorTest, PickerDoesNotReset) {
  // Showing navigation buttons is enabled on iPhone, not on iPad.
  // This test should be skipped if it is not running on an iPhone.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Skipping FormInputAccessoryMediatorTest.PickerReset: The "
                    "test is for iPhone.";
  }

  FormActivityParams params =
      CreateFormActivityParams(/*field_type=*/"select-one");

  OCMExpect([consumer_ showNavigationButtons]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  [consumer_ verify];
}

// Tests consumer and handler are not reset when a field is text.
TEST_F(FormInputAccessoryMediatorTest, TextDoesNotReset) {
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  [[handler_ reject] resetFormInputView];
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
}

// Tests that suggestions are updated and shown.
TEST_F(FormInputAccessoryMediatorTest, ShowSuggestions_NotStateless) {
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitAndDisableFeature(kStatelessFormSuggestionController);

  id providerMock = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectProvider:providerMock];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  __block FormSuggestionsReadyCompletion suggestionsQueryCompletion;

  OCMStub([providerMock
      retrieveSuggestionsForForm:params
                        webState:web_state_list_.GetActiveWebState()
        accessoryViewUpdateBlock:CopyValueToVariable(
                                     suggestionsQueryCompletion)]);
  OCMStub([providerMock mainFillingProduct])
      .andReturn(autofill::FillingProduct::kAutocomplete);

  // Emit a form registration event to trigger the show suggestions code path.
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"value"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  NSArray<FormSuggestion*>* suggestions = [NSArray arrayWithObject:suggestion];

  // Expect to update the view model with the suggestions from the query.
  OCMExpect([consumer_ showAccessorySuggestions:suggestions]);

  // Run the completion block to trigger the code path that updates suggestion
  // in the view model.
  suggestionsQueryCompletion(suggestions, providerMock);

  EXPECT_EQ(autofill::FillingProduct::kAutocomplete,
            mediator_.currentProviderMainFillingProduct);

  EXPECT_OCMOCK_VERIFY(providerMock);
}

// Tests showing suggestions when Stateless is enabled.
TEST_F(FormInputAccessoryMediatorTest, ShowSuggestions) {
  id providerMock = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectProvider:providerMock];

  TestFormSuggestionProvider* testSuggestionProvider =
      [[TestFormSuggestionProvider alloc] init];
  // Set a provider type that doesn't reach a code path that can't be handled in
  // test.
  [testSuggestionProvider
      setType:SuggestionProviderType::SuggestionProviderTypeUnknown];
  [testSuggestionProvider
      setMainFillingProduct:autofill::FillingProduct::kAutocomplete];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");

  __block FormSuggestionsReadyCompletion suggestionsQueryCompletion;

  OCMStub([providerMock
      retrieveSuggestionsForForm:params
                        webState:web_state_list_.GetActiveWebState()
        accessoryViewUpdateBlock:CopyValueToVariable(
                                     suggestionsQueryCompletion)]);

  // Emit a form registration event to trigger the show suggestions code path.
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"value"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  suggestion = [FormSuggestion copy:suggestion
                       andSetParams:params
                           provider:testSuggestionProvider];
  NSArray<FormSuggestion*>* suggestions = [NSArray arrayWithObject:suggestion];

  // Expect to update the view model with the suggestions from the query.
  OCMExpect([consumer_ showAccessorySuggestions:suggestions]);

  // Run the completion block to trigger the code path that updates suggestion
  // in the view model.
  suggestionsQueryCompletion(suggestions, providerMock);

  EXPECT_EQ(autofill::FillingProduct::kAutocomplete,
            mediator_.currentProviderMainFillingProduct);

  EXPECT_OCMOCK_VERIFY(providerMock);
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
        accessoryViewUpdateBlock:[OCMArg checkWithBlock:^BOOL(
                                             FormSuggestionsReadyCompletion
                                                 completion) {
          [suggestionsCompletionsQueue addObject:[completion copy]];
          return YES;
        }]]);

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
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  NSArray<FormSuggestion*>* suggestions_from_first_query =
      [NSArray arrayWithObject:suggestion1];

  FormSuggestion* suggestion2 = [FormSuggestion
      suggestionWithValue:@"value2"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
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

  EXPECT_OCMOCK_VERIFY(providerMock);
}

// Tests that selecting a suggestion when Stateless is enabled is correctly
// handled when no reauthentication is needed.
TEST_F(FormInputAccessoryMediatorTest, DidSelectSuggestion_NoReauth) {

  base::HistogramTester histogram_tester;

  id formInputSuggestionProviderMock =
      OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectCurrentProvider:formInputSuggestionProviderMock];

  // Make a credit card suggestion that wraps all the information needed by
  // Stateless.
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  TestFormSuggestionProvider* testSuggestionProvider =
      [[TestFormSuggestionProvider alloc] init];
  [testSuggestionProvider
      setType:SuggestionProviderType::SuggestionProviderTypeAutofill];
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"value"
       displayDescription:@"display-description"
                     icon:nil
                     type:autofill::SuggestionType::kCreditCardEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  suggestion = [FormSuggestion copy:suggestion
                       andSetParams:params
                           provider:testSuggestionProvider];

  const NSInteger suggestionIndex = 0;

  OCMExpect([formInputSuggestionProviderMock
      didSelectSuggestion:[OCMArg any]
                  atIndex:suggestionIndex]);

  [mediator_ didSelectSuggestion:suggestion atIndex:suggestionIndex];

  // Look the authentication metrics associated with the type of the selected
  // provided are correctly recorded.
  histogram_tester.ExpectTotalCount("IOS.Reauth.CreditCard.Autofill", 2);
  histogram_tester.ExpectBucketCount("IOS.Reauth.CreditCard.Autofill",
                                     /*sample=*/
                                     static_cast<base::HistogramBase::Sample32>(
                                         ReauthenticationEvent::kAttempt),
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("IOS.Reauth.CreditCard.Autofill",
                                     /*sample=*/
                                     static_cast<base::HistogramBase::Sample32>(
                                         ReauthenticationEvent::kSuccess),
                                     /*expected_count=*/1);

  EXPECT_OCMOCK_VERIFY(formInputSuggestionProviderMock);
}
