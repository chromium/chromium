// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/form_input_accessory_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/filling/filling_product.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/form_input_accessory_mediator+testing.h"
#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/keyboard_accessory_optional_update_scheduler.h"
#import "ios/chrome/browser/autofill/form_input_accessory/ui/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
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

// A period for unit tests to fast forward time to observe the result of
// optional updates. This is used to wait out cooldowns and delays. The 10ms
// buffer is added to create a slighly longer time delta.
const base::TimeDelta kDelayForAcceptingOptionalUpdates =
    kOptionalUpdateCooldownPeriod + base::Milliseconds(10);

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

void PostKeyboardWillShowNotifications(int count = 1) {
  for (int i = 0; i < count; ++i) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIKeyboardWillShowNotification
                      object:nil];
  }
}

FormSuggestion* CreateFormSuggestion(NSString* value) {
  return [FormSuggestion
      suggestionWithValue:value
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
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

    test_web_state_->WasShown();

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

  void CaptureAccessorySuggestions() {
    OCMStub(
        [consumer_ showAccessorySuggestions:[OCMArg checkWithBlock:^BOOL(
                                                        NSArray* suggestions) {
                     received_suggestions_ = suggestions;
                     return YES;
                   }]]);
  }

  void SetUpProviderWithSuggestions(const FormActivityParams& params,
                                    NSArray<FormSuggestion*>* suggestions) {
    provider_ = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
    [mediator_ injectProvider:provider_];

    OCMStub(
        [provider_
            retrieveSuggestionsForForm:params
                              webState:web_state_list_.GetActiveWebState()
              accessoryViewUpdateBlock:[OCMArg
                                           checkWithBlock:^BOOL(
                                               FormSuggestionsReadyCompletion
                                                   completion) {
                                             completion(suggestions, provider_);
                                             return YES;
                                           }]])
        .ignoringNonObjectArgs();
    OCMStub([provider_ mainFillingProduct])
        .andReturn(autofill::FillingProduct::kAutocomplete);
  }

  NSMutableArray<FormSuggestionsReadyCompletion>*
  SetUpProviderWithPendingSuggestionQueries(const FormActivityParams& params) {
    provider_ = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
    [mediator_ injectProvider:provider_];

    NSMutableArray<FormSuggestionsReadyCompletion>* completions =
        [NSMutableArray array];
    OCMStub(
        [provider_
            retrieveSuggestionsForForm:params
                              webState:web_state_list_.GetActiveWebState()
              accessoryViewUpdateBlock:[OCMArg
                                           checkWithBlock:^BOOL(
                                               FormSuggestionsReadyCompletion
                                                   completion) {
                                             [completions
                                                 addObject:[completion copy]];
                                             return YES;
                                           }]])
        .ignoringNonObjectArgs();
    OCMStub([provider_ mainFillingProduct])
        .andReturn(autofill::FillingProduct::kAutocomplete);
    return completions;
  }

  web::FakeWebState* GetActiveFakeWebState() {
    return static_cast<web::FakeWebState*>(web_state_list_.GetActiveWebState());
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  std::unique_ptr<web::FakeWebFrame> main_frame_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  id consumer_;
  id handler_;
  id provider_;
  NSArray* received_suggestions_ = nil;
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

// Tests that form activities on pages whose URLs don't have a web scheme are
// ignored and reset the accessory.
TEST_F(FormInputAccessoryMediatorTest,
       FormActivityShouldBeIgnoredWhenNotWebScheme) {
  CaptureAccessorySuggestions();
  GetActiveFakeWebState()->SetCurrentURL(GURL("about:blank"));

  OCMExpect([handler_ resetFormInputView]);
  test_form_activity_tab_helper_.FormActivityRegistered(
      main_frame_.get(), CreateFormActivityParams(/*field_type=*/"text"));

  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that form activities on non-HTML pages are ignored and reset the
// accessory.
TEST_F(FormInputAccessoryMediatorTest, FormActivityShouldBeIgnoredWhenNotHtml) {
  CaptureAccessorySuggestions();
  GetActiveFakeWebState()->SetContentIsHTML(false);

  OCMExpect([handler_ resetFormInputView]);
  test_form_activity_tab_helper_.FormActivityRegistered(
      main_frame_.get(), CreateFormActivityParams(/*field_type=*/"text"));

  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that the suggestions are reset when a navigation is finished.
TEST_F(FormInputAccessoryMediatorTest,
       NavigationShouldRestoreKeyboardAccessoryView) {
  CaptureAccessorySuggestions();
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  SetUpProviderWithSuggestions(params, @[ CreateFormSuggestion(@"foo") ]);

  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  ASSERT_TRUE(received_suggestions_.count);

  OCMExpect([handler_ resetFormInputView]);
  web::FakeNavigationContext navigation_context;
  GetActiveFakeWebState()->OnNavigationFinished(&navigation_context);

  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that the suggestions are not reset when a finished navigation happened
// within the same document.
TEST_F(FormInputAccessoryMediatorTest,
       SameDocumentNavigationShouldNotResetKeyboardAccessorySuggestions) {
  CaptureAccessorySuggestions();
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  SetUpProviderWithSuggestions(params, @[ CreateFormSuggestion(@"foo") ]);

  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  NSUInteger initial_suggestion_count = received_suggestions_.count;
  ASSERT_TRUE(initial_suggestion_count);

  [[handler_ reject] resetFormInputView];
  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(true);
  GetActiveFakeWebState()->OnNavigationFinished(&navigation_context);

  EXPECT_EQ(received_suggestions_.count, initial_suggestion_count);
}

// Tests that "blur" events are ignored.
TEST_F(FormInputAccessoryMediatorTest, FormActivityBlurShouldBeIgnored) {
  CaptureAccessorySuggestions();
  provider_ = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectProvider:provider_];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  params.type = "blur";
  [[handler_ reject] resetFormInputView];
  [[provider_ reject] retrieveSuggestionsForForm:params
                                        webState:static_cast<web::WebState*>(
                                                     [OCMArg anyPointer])
                        accessoryViewUpdateBlock:[OCMArg any]];
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  EXPECT_FALSE(received_suggestions_.count);
  EXPECT_OCMOCK_VERIFY(provider_);
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

// Tests that the autofill suggestion IPH is triggered when suggesting an
// address if the suggestion's `featureForiPH` property is set.
TEST_F(FormInputAccessoryMediatorTest, AutofillSuggestionIPH) {
  TestFormSuggestionProvider* testSuggestionProvider =
      [[TestFormSuggestionProvider alloc] init];
  [testSuggestionProvider
      setType:SuggestionProviderType::SuggestionProviderTypeAutofill];

  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  FormSuggestion* suggestion = CreateFormSuggestion(@"foo");
  suggestion.featureForIPH =
      SuggestionFeatureForIPH::kAutofillExternalAccountProfile;
  suggestion = [FormSuggestion copy:suggestion
                       andSetParams:params
                           provider:testSuggestionProvider];
  SetUpProviderWithSuggestions(params, @[ suggestion ]);
  id<FormInputSuggestionsProvider> typed_provider = provider_;
  OCMStub([typed_provider type]).andReturn(SuggestionProviderTypeAutofill);

  OCMExpect(
      [handler_ showAutofillSuggestionIPHIfNeededFor:suggestion.featureForIPH]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  EXPECT_OCMOCK_VERIFY(handler_);
}

// Tests that only the suggestions from the latest query in concurrent queries
// are updated and shown.
TEST_F(FormInputAccessoryMediatorTest, ShowSuggestions_WithConcurrentQueries) {
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  NSMutableArray<FormSuggestionsReadyCompletion>* suggestionsCompletionsQueue =
      SetUpProviderWithPendingSuggestionQueries(params);

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
   objectAtIndex:0](suggestions_from_first_query, provider_);
  [suggestionsCompletionsQueue
   objectAtIndex:1](suggestions_from_second_query, provider_);

  EXPECT_OCMOCK_VERIFY(provider_);
}

// Tests that only the latest query can update the consumer when concurrent
// queries return no suggestions.
TEST_F(FormInputAccessoryMediatorTest,
       ShowEmptySuggestions_WithConcurrentQueries) {
  CaptureAccessorySuggestions();
  FormActivityParams params = CreateFormActivityParams(/*field_type=*/"text");
  NSMutableArray<FormSuggestionsReadyCompletion>* suggestionsCompletionsQueue =
      SetUpProviderWithPendingSuggestionQueries(params);

  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  ASSERT_EQ([suggestionsCompletionsQueue count], 2ul);

  [suggestionsCompletionsQueue objectAtIndex:0](@[], provider_);
  EXPECT_FALSE(received_suggestions_);

  [suggestionsCompletionsQueue objectAtIndex:1](@[], provider_);
  ASSERT_TRUE(received_suggestions_);
  EXPECT_EQ(0U, received_suggestions_.count);

  EXPECT_OCMOCK_VERIFY(provider_);
}

// Tests that selecting a suggestion when Stateless is enabled is correctly
// handled when no reauthentication is needed.
TEST_F(FormInputAccessoryMediatorTest, DidSelectSuggestion_NoReauth) {
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

  OCMExpect([formInputSuggestionProviderMock didSelectSuggestion:[OCMArg any]
                                                         atIndex:suggestionIndex
                                                      completion:[OCMArg any]]);

  [mediator_ didSelectSuggestion:suggestion
                         atIndex:suggestionIndex
                      completion:nil];

  EXPECT_OCMOCK_VERIFY(formInputSuggestionProviderMock);
}

// Tests that selecting a suggestion after disconnect does not trigger anything
// and does not forward to the provider.
TEST_F(FormInputAccessoryMediatorTest, DidSelectSuggestion_AfterDisconnect) {
  id formInputSuggestionProviderMock =
      OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ injectCurrentProvider:formInputSuggestionProviderMock];

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

  // Disconnect the mediator first.
  [mediator_ disconnect];

  // We reject any calls to didSelectSuggestion on the provider.
  [[formInputSuggestionProviderMock reject] didSelectSuggestion:[OCMArg any]
                                                        atIndex:suggestionIndex
                                                     completion:[OCMArg any]];

  [mediator_ didSelectSuggestion:suggestion
                         atIndex:suggestionIndex
                      completion:nil];

  EXPECT_OCMOCK_VERIFY(formInputSuggestionProviderMock);
}

// Tests that suggestion refreshes triggered by `keyboardWillShow` are not
// throttled when none of the feature flags are enabled.
// The relationship of the two feature flags used in this test and a few other
// tests below it.
// - `kSuppressKeyboardWillShowSuggestionRefresh` has the highest impact.
//   If it is enabled, it will suppress all suggestion refreshes from
//   `keyboardWillShow`.
// - `kAutofillThrottleOptionalSuggestionRefresh` has a chance to work
//   only when kSuppressKeyboardWillShowSuggestionRefresh is disabled.
// TODO(crbug.com/477866475): Flaky on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_keyboardWillShowRefresh_NotThrottledOrSuppressed \
  keyboardWillShowRefresh_NotThrottledOrSuppressed
#else
#define MAYBE_keyboardWillShowRefresh_NotThrottledOrSuppressed \
  DISABLED_keyboardWillShowRefresh_NotThrottledOrSuppressed
#endif
TEST_F(FormInputAccessoryMediatorTest,
       MAYBE_keyboardWillShowRefresh_NotThrottledOrSuppressed) {
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kSuppressKeyboardWillShowSuggestionRefresh,
                             kAutofillThrottleOptionalSuggestionRefresh});

  FormActivityParams params = CreateFormActivityParams("text");
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);

  FormInputAccessoryMediator* mock_mediator_ = OCMPartialMock(mediator_);
  __block int count = 0;
  OCMStub([mock_mediator_
              retrieveSuggestionsForForm:params
                                webState:static_cast<web::WebState*>(
                                             [OCMArg anyPointer])])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation*) {
        count++;
      });

  PostKeyboardWillShowNotifications(3);
  EXPECT_EQ(count, 3);
}

// Tests that suggestion refreshes triggered by keyboardWillShow are throttled
// when only kAutofillThrottleOptionalSuggestionRefresh is enabled.
// TODO(crbug.com/477866475): Flaky on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_keyboardWillShowRefresh_Throttled \
  keyboardWillShowRefresh_Throttled
#else
#define MAYBE_keyboardWillShowRefresh_Throttled \
  DISABLED_keyboardWillShowRefresh_Throttled
#endif
TEST_F(FormInputAccessoryMediatorTest,
       MAYBE_keyboardWillShowRefresh_Throttled) {
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitWithFeatures(
      /*enabled_features=*/{kAutofillThrottleOptionalSuggestionRefresh},
      /*disabled_features=*/{kSuppressKeyboardWillShowSuggestionRefresh});

  FormActivityParams params = CreateFormActivityParams("text");
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  task_environment_.FastForwardBy(kDelayForAcceptingOptionalUpdates);

  FormInputAccessoryMediator* mock_mediator_ = OCMPartialMock(mediator_);
  __block int count = 0;
  OCMStub([mock_mediator_
              retrieveSuggestionsForForm:params
                                webState:static_cast<web::WebState*>(
                                             [OCMArg anyPointer])])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation*) {
        count++;
      });

  PostKeyboardWillShowNotifications(3);
  // Update isn't immediately triggered -- delayed.
  EXPECT_EQ(count, 0);

  task_environment_.FastForwardBy(kOptionalUpdateDelay +
                                  base::Milliseconds(10));
  // Update is triggered once -- throttled.
  EXPECT_EQ(count, 1);
}

// Tests that suggestion refreshes triggered by keyboardWillShow restarts the
// delay when throttled.
// TODO(crbug.com/477866475): Flaky on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_keyboardWillShowRefresh_Throttled_RollingOver \
  keyboardWillShowRefresh_Throttled_RollingOver
#else
#define MAYBE_keyboardWillShowRefresh_Throttled_RollingOver \
  DISABLED_keyboardWillShowRefresh_Throttled_RollingOver
#endif
TEST_F(FormInputAccessoryMediatorTest,
       MAYBE_keyboardWillShowRefresh_Throttled_RollingOver) {
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitWithFeatures(
      /*enabled_features=*/{kAutofillThrottleOptionalSuggestionRefresh},
      /*disabled_features=*/{kSuppressKeyboardWillShowSuggestionRefresh});

  FormActivityParams params = CreateFormActivityParams("text");
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  task_environment_.FastForwardBy(kDelayForAcceptingOptionalUpdates);

  FormInputAccessoryMediator* mock_mediator_ = OCMPartialMock(mediator_);
  __block int count = 0;
  OCMStub([mock_mediator_
              retrieveSuggestionsForForm:params
                                webState:static_cast<web::WebState*>(
                                             [OCMArg anyPointer])])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation*) {
        count++;
      });

  const base::TimeDelta halfDelay =
      kOptionalUpdateDelay / 2 + base::Milliseconds(10);

  // keyboardWillShow notification should not trigger a refresh immediately.
  PostKeyboardWillShowNotifications(1);
  EXPECT_EQ(count, 0);

  // After half of the delay, no refreshes should be triggered.
  task_environment_.FastForwardBy(halfDelay);
  EXPECT_EQ(count, 0);

  // A new notification restarts the delay. So, no refreshes should be triggered
  // after the delay since the first event.
  PostKeyboardWillShowNotifications(1);
  task_environment_.FastForwardBy(halfDelay);
  EXPECT_EQ(count, 0);

  // After another half delay, the refresh should now be triggered.
  task_environment_.FastForwardBy(halfDelay);
  EXPECT_EQ(count, 1);
}

// Tests that suggestion refreshes triggered by keyboardWillShow are suppressed
// when kSuppressKeyboardWillShowSuggestionRefresh is enabled.
TEST_F(FormInputAccessoryMediatorTest, keyboardWillShowRefresh_Suppressed) {
  base::test::ScopedFeatureList scoped_featurelist;
  scoped_featurelist.InitWithFeatures(
      /*enabled_features=*/{kSuppressKeyboardWillShowSuggestionRefresh},
      /*disabled_features=*/{kAutofillThrottleOptionalSuggestionRefresh});

  FormActivityParams params = CreateFormActivityParams("text");
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame_.get(),
                                                        params);
  task_environment_.FastForwardBy(kDelayForAcceptingOptionalUpdates);

  FormInputAccessoryMediator* mock_mediator_ = OCMPartialMock(mediator_);
  __block int count = 0;
  OCMStub([mock_mediator_
              retrieveSuggestionsForForm:params
                                webState:static_cast<web::WebState*>(
                                             [OCMArg anyPointer])])
      .ignoringNonObjectArgs()
      .andDo(^(NSInvocation*) {
        count++;
      });

  PostKeyboardWillShowNotifications(3);
  EXPECT_EQ(count, 0);

  task_environment_.FastForwardBy(kDelayForAcceptingOptionalUpdates);
  EXPECT_EQ(count, 0);
}
