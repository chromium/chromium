// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import "base/memory/raw_ptr.h"
#import "base/test/test_future.h"
#import "base/unguessable_token.h"
#import "base/values.h"
#import "components/autofill/core/browser/actor/mock_actor_form_filling_service.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Adds a `FormFillingRequest` proto to `action`.
void AddFormFillingRequestToAction(
    optimization_guide::proto::AttemptFormFillingAction& action,
    bool is_address,
    bool use_coordinates,
    const std::string& document_identifier = "") {
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      is_address
          ? optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS
          : optimization_guide::proto::
                FormFillingRequest_RequestedData_CREDIT_CARD);
  auto* field = request->add_trigger_fields();
  if (use_coordinates) {
    field->mutable_coordinate()->set_x(is_address ? 10 : 30);
    field->mutable_coordinate()->set_y(is_address ? 20 : 40);
  } else {
    field->set_content_node_id(is_address ? 100 : 200);
    field->mutable_document_identifier()->set_serialized_token(
        document_identifier);
  }
}

// Creates an `AttemptFormFillingAction` with a single request.
optimization_guide::proto::AttemptFormFillingAction CreateAction(
    bool is_address = true,
    bool use_coordinates = true,
    const std::string& document_identifier = "") {
  optimization_guide::proto::AttemptFormFillingAction action;
  AddFormFillingRequestToAction(action, is_address, use_coordinates,
                                document_identifier);
  return action;
}

// Creates an address `ActorFormFillingRequest` for testing.
autofill::ActorFormFillingRequest CreateAddressFormFillingRequest() {
  autofill::ActorFormFillingRequest request;
  request.requested_data = autofill::ActorFormFillingRequestedData::kAddress;
  request.request_origin = url::Origin::Create(GURL("https://example.com"));

  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(123);
  suggestion.title = "John Doe";
  suggestion.details = "123 Main St";
  request.suggestions.push_back(suggestion);
  return request;
}

// Creates a credit card `ActorFormFillingRequest` for testing.
autofill::ActorFormFillingRequest CreateCreditCardFormFillingRequest() {
  autofill::ActorFormFillingRequest request;
  request.requested_data = autofill::ActorFormFillingRequestedData::kCreditCard;
  request.request_origin = url::Origin::Create(GURL("https://example.com"));

  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(456);
  suggestion.title = "Visa";
  suggestion.details = "1111";
  request.suggestions.push_back(suggestion);
  return request;
}

}  // namespace

// A fake implementation of ActorTaskInterventionDelegate.
@interface FakeFormFillingTaskInterventionDelegate
    : NSObject <ActorTaskInterventionDelegate>
@property(nonatomic, assign, readonly) BOOL selectFromSuggestionsCalled;
@property(nonatomic, strong, readonly)
    NSArray<ActorFormSuggestion*>* promptedSuggestions;
@end

@implementation FakeFormFillingTaskInterventionDelegate {
  void (^_completionHandler)(ActorFormSuggestion*, BOOL);
}

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:(void (^)(ActorFormSuggestion* selectedSuggestion,
                                    BOOL obsoleteFlag))completionHandler {
  _selectFromSuggestionsCalled = YES;
  _promptedSuggestions = suggestions;
  _completionHandler = [completionHandler copy];
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion {
  if (_completionHandler) {
    void (^completion)(ActorFormSuggestion*, BOOL) = _completionHandler;
    _completionHandler = nil;
    completion(selectedSuggestion, /*obsoleteFlag=*/NO);
  }
}
@end

namespace actor {

// Unit test fixture for `AttemptFormFillingTool`.
class AttemptFormFillingToolTest : public PlatformTest {
 public:
  AttemptFormFillingToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    auto mock_service =
        std::make_unique<autofill::MockActorFormFillingService>();
    mock_form_filling_service_ptr_ = mock_service.get();

    auto form_filling_handler = ActorTaskFormFillingHandler::CreateForTesting(
        ActorTaskId(1), /*login_service=*/nullptr, std::move(mock_service));
    form_filling_handler_ptr_ = form_filling_handler.get();

    intervention_delegate_ =
        [[FakeFormFillingTaskInterventionDelegate alloc] init];
    form_filling_handler_ptr_->SetInterventionDelegateForTesting(
        intervention_delegate_);

    delegate_.set_form_filling_handler(std::move(form_filling_handler));

    base::DictValue success_dict;
    success_dict.Set("resultCode", 0);
    success_js_result_ = base::Value(std::move(success_dict));

    base::DictValue renderer_ids_dict;
    renderer_ids_dict.Set("resultCode", 0);
    base::ListValue unique_ids;
    unique_ids.Append("1");
    renderer_ids_dict.Set("uniqueIds", std::move(unique_ids));
    success_renderer_ids_js_result_ = base::Value(std::move(renderer_ids_dict));
  }

  // Gets the current web state list.
  WebStateList* GetWebStateList() { return browser_->GetWebStateList(); }

  // Creates the `AttemptFormFillingTool` from `action`.
  std::unique_ptr<AttemptFormFillingTool> CreateTool(
      const optimization_guide::proto::AttemptFormFillingAction& action,
      web::WebState* web_state) {
    return AttemptFormFillingTool::Create(web_state->GetWeakPtr(), action,
                                          &delegate_);
  }

  // Adds a JS result for a specific function call to the main frame of the
  // given web state.
  void AddJsResultForFunctionCallToMainFrame(web::FakeWebState* web_state,
                                             base::Value* js_result,
                                             const std::string& function_name) {
    web::FakeWebFramesManager* frames_manager =
        static_cast<web::FakeWebFramesManager*>(web_state->GetWebFramesManager(
            ActionTargetJavaScriptFeature::GetInstance()
                ->GetSupportedContentWorld()));
    web::FakeWebFrame* main_frame =
        static_cast<web::FakeWebFrame*>(frames_manager->GetMainWebFrame());
    CHECK(main_frame);
    main_frame->AddJsResultForFunctionCall(js_result, function_name);
  }

  // Configures the web state with a main web frame.
  void SetUpMainFrame(web::FakeWebState* web_state) {
    web::test::OverrideJavaScriptFeatures(
        profile_.get(),
        {
            ActionTargetJavaScriptFeature::GetInstance(),
            AttemptFormFillingToolJavaScriptFeature::GetInstance(),
        });
    auto page_content_frames_manager =
        std::make_unique<web::FakeWebFramesManager>();
    web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                   std::move(page_content_frames_manager));
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
    main_frame->set_browser_state(profile_.get());
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(ActionTargetJavaScriptFeature::GetInstance()
                                       ->GetSupportedContentWorld(),
                                   std::move(frames_manager));
    AddJsResultForFunctionCallToMainFrame(web_state, &success_js_result_,
                                          "action_target.resolveTargetIframe");
    AddJsResultForFunctionCallToMainFrame(
        web_state, &success_renderer_ids_js_result_,
        "attempt_form_filling.getAutofillRendererIds");
  }

  // Helper method to add a new tab the tool will actuate upon.
  web::FakeWebState* CreateAndInsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());

    web_state->SetNavigationManager(std::move(navigation_manager));

    web::FakeWebState* web_state_ptr = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

    SetUpMainFrame(web_state_ptr);

    autofill_client_ =
        std::make_unique<autofill::TestAutofillClientIOS>(web_state_ptr, nil);
    return web_state_ptr;
  }

  // Convenience getter methods.
  autofill::MockActorFormFillingService* mock_form_filling_service() {
    return mock_form_filling_service_ptr_;
  }
  ActorTaskFormFillingHandler* form_filling_handler() {
    return form_filling_handler_ptr_;
  }
  FakeFormFillingTaskInterventionDelegate* intervention_delegate() {
    return intervention_delegate_;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeToolDelegate delegate_;
  raw_ptr<autofill::MockActorFormFillingService>
      mock_form_filling_service_ptr_ = nullptr;
  raw_ptr<ActorTaskFormFillingHandler> form_filling_handler_ptr_ = nullptr;
  FakeFormFillingTaskInterventionDelegate* intervention_delegate_ = nil;
  std::unique_ptr<autofill::TestAutofillClientIOS> autofill_client_;
  base::Value success_js_result_;
  base::Value success_renderer_ids_js_result_;
};

// Test that successful tool execution with coordinate targets retrieves
// suggestions, prompts the user, fills the selected suggestion, and completes
// successfully.
TEST_F(AttemptFormFillingToolTest, Success_Coordinates) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  // Set up 2 renderer IDs for the 2 trigger fields in this test.
  base::DictValue renderer_ids_dict;
  renderer_ids_dict.Set("resultCode", 0);
  base::ListValue unique_ids;
  unique_ids.Append("1");
  unique_ids.Append("2");
  renderer_ids_dict.Set("uniqueIds", std::move(unique_ids));
  base::Value renderer_ids_result(std::move(renderer_ids_dict));
  AddJsResultForFunctionCallToMainFrame(
      web_state, &renderer_ids_result,
      "attempt_form_filling.getAutofillRendererIds");

  optimization_guide::proto::AttemptFormFillingAction action;
  AddFormFillingRequestToAction(action, /*is_address=*/true,
                                /*use_coordinates=*/true);
  AddFormFillingRequestToAction(action, /*is_address=*/false,
                                /*use_coordinates=*/true);

  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  std::vector<autofill::ActorFormFillingRequest> suggestions_result = {
      CreateAddressFormFillingRequest(),
      CreateCreditCardFormFillingRequest(),
  };

  // Expect `GetSuggestions` to be called with callback.
  base::test::TestFuture<void> get_suggestions_called_future;
  EXPECT_CALL(*mock_form_filling_service(),
              GetSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](autofill::AutofillClient& client,
              base::span<const autofill::ActorFormFillingService::FillRequest>
                  fill_requests,
              autofill::ActorFormFillingService::GetSuggestionsCallback
                  callback) {
            std::move(callback).Run(std::move(suggestions_result));
            // Mark `GetSuggestions` as called to exit the blocking run loop.
            get_suggestions_called_future.SetValue();
          });

  // Set expectations for when the suggestions are retrieved.
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 0));

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Wait for the tool to resolve target frame and fetch suggestions.
  EXPECT_TRUE(get_suggestions_called_future.Wait());

  // --- Step 1: Handle first suggestion (Address) ---
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(intervention_delegate().promptedSuggestions.count, 1u);

  ActorFormSuggestion* selected_suggestion0 =
      intervention_delegate().promptedSuggestions[0];
  EXPECT_EQ(selected_suggestion0.type, autofill::SuggestionType::kAddressEntry);

  // Expect FillForm to be called for index 0.
  EXPECT_CALL(*mock_form_filling_service(),
              FillForm(testing::_, 0, testing::_));

  // Expect ScrollToForm to be called for index 1 when first selection is done.
  // Block the runloop until the scroll has happened to avoid early exit.
  base::test::TestFuture<void> second_scroll_future;
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 1))
      .WillOnce([&] { second_scroll_future.SetValue(); });

  // Run the first selection.
  [intervention_delegate() runCompletionWithSuggestion:selected_suggestion0];

  // Wait for first selection to be processed and second request prompted.
  EXPECT_TRUE(second_scroll_future.Wait());

  // --- Step 2: Handle second suggestion (Credit Card) ---
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(intervention_delegate().promptedSuggestions.count, 1u);

  ActorFormSuggestion* selected_suggestion1 =
      intervention_delegate().promptedSuggestions[0];
  EXPECT_EQ(selected_suggestion1.type,
            autofill::SuggestionType::kCreditCardEntry);

  // Expect FillForm to be called for index 1.
  EXPECT_CALL(*mock_form_filling_service(),
              FillForm(testing::_, 1, testing::_));

  // Expect FillSuggestions to be called.
  EXPECT_CALL(*mock_form_filling_service(),
              FillSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [](autofill::AutofillClient& client,
             base::span<const autofill::ActorFormFillingSelection>
                 chosen_suggestions,
             base::OnceCallback<void(
                 base::expected<std::string, autofill::ActorFormFillingError>)>
                 callback) { std::move(callback).Run(""); });

  // Run the second selection.
  [intervention_delegate() runCompletionWithSuggestion:selected_suggestion1];

  // Verify the execution outcome.
  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

// Test that successful tool execution with DOM node targets retrieves
// suggestions, prompts the user, fills the selected suggestion, and completes
// successfully.
TEST_F(AttemptFormFillingToolTest, Success_DomNode) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  web::WebFramesManager* frames_manager =
      ActionTargetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state);
  web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_frame);

  base::UnguessableToken remote_token = base::UnguessableToken::Create();
  std::optional<base::UnguessableToken> local_token =
      autofill::DeserializeJavaScriptFrameId(main_frame->GetFrameId());
  ASSERT_TRUE(local_token.has_value());

  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state);
  registrar->RegisterMapping(autofill::RemoteFrameToken(remote_token),
                             autofill::LocalFrameToken(*local_token));

  // Set up 2 renderer IDs for the 2 trigger fields in this test.
  base::DictValue renderer_ids_dict;
  renderer_ids_dict.Set("resultCode", 0);
  base::ListValue unique_ids;
  unique_ids.Append("1");
  unique_ids.Append("2");
  renderer_ids_dict.Set("uniqueIds", std::move(unique_ids));
  base::Value renderer_ids_result(std::move(renderer_ids_dict));
  AddJsResultForFunctionCallToMainFrame(
      web_state, &renderer_ids_result,
      "attempt_form_filling.getAutofillRendererIds");

  optimization_guide::proto::AttemptFormFillingAction action;
  AddFormFillingRequestToAction(action, /*is_address=*/true,
                                /*use_coordinates=*/false,
                                remote_token.ToString());
  AddFormFillingRequestToAction(action, /*is_address=*/false,
                                /*use_coordinates=*/false,
                                remote_token.ToString());

  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  std::vector<autofill::ActorFormFillingRequest> suggestions_result = {
      CreateAddressFormFillingRequest(),
      CreateCreditCardFormFillingRequest(),
  };

  // Expect `GetSuggestions` to be called with callback.
  base::test::TestFuture<void> get_suggestions_called_future;
  EXPECT_CALL(*mock_form_filling_service(),
              GetSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](autofill::AutofillClient& client,
              base::span<const autofill::ActorFormFillingService::FillRequest>
                  fill_requests,
              autofill::ActorFormFillingService::GetSuggestionsCallback
                  callback) {
            std::move(callback).Run(std::move(suggestions_result));
            // Mark `GetSuggestions` as called to exit the blocking run loop.
            get_suggestions_called_future.SetValue();
          });

  // Set expectations for when the suggestions are retrieved.
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 0));

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Wait for the tool to resolve target frame and fetch suggestions.
  EXPECT_TRUE(get_suggestions_called_future.Wait());

  // --- Step 1: Handle first suggestion (Address) ---
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(intervention_delegate().promptedSuggestions.count, 1u);

  ActorFormSuggestion* selected_suggestion0 =
      intervention_delegate().promptedSuggestions[0];
  EXPECT_EQ(selected_suggestion0.type, autofill::SuggestionType::kAddressEntry);

  // Expect FillForm to be called for index 0.
  EXPECT_CALL(*mock_form_filling_service(),
              FillForm(testing::_, 0, testing::_));

  // Expect ScrollToForm to be called for index 1 when first selection is done.
  // Block the runloop until the scroll has happened to avoid early exit.
  base::test::TestFuture<void> second_scroll_future;
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 1))
      .WillOnce([&] { second_scroll_future.SetValue(); });

  // Run the first selection.
  [intervention_delegate() runCompletionWithSuggestion:selected_suggestion0];

  // Wait for first selection to be processed and second request prompted.
  EXPECT_TRUE(second_scroll_future.Wait());

  // --- Step 2: Handle second suggestion (Credit Card) ---
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(intervention_delegate().promptedSuggestions.count, 1u);

  ActorFormSuggestion* selected_suggestion1 =
      intervention_delegate().promptedSuggestions[0];
  EXPECT_EQ(selected_suggestion1.type,
            autofill::SuggestionType::kCreditCardEntry);

  // Expect FillForm to be called for index 1.
  EXPECT_CALL(*mock_form_filling_service(),
              FillForm(testing::_, 1, testing::_));

  // Expect FillSuggestions to be called.
  EXPECT_CALL(*mock_form_filling_service(),
              FillSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [](autofill::AutofillClient& client,
             base::span<const autofill::ActorFormFillingSelection>
                 chosen_suggestions,
             base::OnceCallback<void(
                 base::expected<std::string, autofill::ActorFormFillingError>)>
                 callback) { std::move(callback).Run(""); });

  // Run the second selection.
  [intervention_delegate() runCompletionWithSuggestion:selected_suggestion1];

  // Verify the execution outcome.
  ToolExecutionResult result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

// Test that validation fails if the action contains no form filling requests.
TEST_F(AttemptFormFillingToolTest, Validate_EmptyRequestsFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();
  optimization_guide::proto::AttemptFormFillingAction action;

  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());
  ToolExecutionResult result = future.Get();

  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that validation fails if one of the requests has no trigger fields.
TEST_F(AttemptFormFillingToolTest, Validate_OneRequestEmptyTriggerFieldsFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action;
  AddFormFillingRequestToAction(action, /*is_address=*/true,
                                /*use_coordinates=*/true);

  // Request 1: has 0 trigger fields.
  auto* request1 = action.add_form_filling_requests();
  request1->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);

  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that validation fails if one of the requests has an invalid trigger
// field.
TEST_F(AttemptFormFillingToolTest,
       Validate_OneRequestInvalidTriggerFieldFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action;
  AddFormFillingRequestToAction(action, /*is_address=*/true,
                                /*use_coordinates=*/true);

  // Request 1: has 1 invalid trigger field (neither node ID nor coordinate is
  // set).
  auto* request1 = action.add_form_filling_requests();
  request1->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);
  request1->add_trigger_fields();

  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that execution fails if the WebState is destroyed.
TEST_F(AttemptFormFillingToolTest, Execute_WebStateDestroyedFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  GetWebStateList()->CloseWebStateAt(0, WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kTabWentAway);
}

// Test that execution fails if the WebFramesManager is null.
TEST_F(AttemptFormFillingToolTest, Execute_NoWebFramesManagerFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(web_state->GetWebFramesManager(
                ActionTargetJavaScriptFeature::GetInstance()
                    ->GetSupportedContentWorld()),
            nullptr);

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Test that execution fails if there is no main web frame.
TEST_F(AttemptFormFillingToolTest, Execute_NoMainFrameFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      ActionTargetJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_EQ(
      web_state
          ->GetWebFramesManager(ActionTargetJavaScriptFeature::GetInstance()
                                    ->GetSupportedContentWorld())
          ->GetMainWebFrame(),
      nullptr);

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Test that if target frame resolution returns an invalid result, the tool
// execution fails.
TEST_F(AttemptFormFillingToolTest,
       Execute_ResolveTargetFrameInvalidResultFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  base::Value js_result(12345);  // non-dictionary is invalid
  AddJsResultForFunctionCallToMainFrame(web_state, &js_result,
                                        "action_target.resolveTargetIframe");

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

// Test that if resolving the Autofill renderer IDs returns an invalid result,
// the tool execution fails.
TEST_F(AttemptFormFillingToolTest,
       Execute_GetAutofillRendererIdsInvalidResultFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  // Set up invalid result for getAutofillRendererIds (returns a number instead
  // of a list).
  base::Value renderer_id_result(12345);
  AddJsResultForFunctionCallToMainFrame(
      web_state, &renderer_id_result,
      "attempt_form_filling.getAutofillRendererIds");

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

// Test that if resolving Autofill renderer IDs fails because the target is not
// an autofill element, the tool execution fails with kFormFillingFieldNotFound.
TEST_F(AttemptFormFillingToolTest,
       Execute_GetAutofillRendererIdsTargetNotAutofillElementFails) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  // Set up result with resultCode = 4 (kTargetNotAutofillElement).
  base::DictValue renderer_ids_dict;
  renderer_ids_dict.Set("resultCode", 4);
  base::Value renderer_ids_result(std::move(renderer_ids_dict));
  AddJsResultForFunctionCallToMainFrame(
      web_state, &renderer_ids_result,
      "attempt_form_filling.getAutofillRendererIds");

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFormFillingFieldNotFound);
}

// Test that tool execution fails if suggestion retrieval returns an error.
TEST_F(AttemptFormFillingToolTest, Execute_GetSuggestionsError) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  EXPECT_CALL(*mock_form_filling_service(),
              GetSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [](autofill::AutofillClient& client,
             base::span<const autofill::ActorFormFillingService::FillRequest>
                 fill_requests,
             autofill::ActorFormFillingService::GetSuggestionsCallback
                 callback) {
            std::move(callback).Run(base::unexpected(
                autofill::ActorFormFillingError::kNoSuggestions));
          });

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(),
            mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable);
}

// Test that if the dialog is closed without selecting a suggestion, the tool
// execution fails with kFormFillingDialogError.
TEST_F(AttemptFormFillingToolTest, Execute_DialogClosedError) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  std::vector<autofill::ActorFormFillingRequest> suggestions_result = {
      CreateAddressFormFillingRequest(),
  };

  // Expect that `GetSuggestions` gets called with callback. Block the runloop
  // until the callback is invoked.
  base::test::TestFuture<void> get_suggestions_called_future;
  EXPECT_CALL(*mock_form_filling_service(),
              GetSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](autofill::AutofillClient& client,
              base::span<const autofill::ActorFormFillingService::FillRequest>
                  fill_requests,
              autofill::ActorFormFillingService::GetSuggestionsCallback
                  callback) {
            std::move(callback).Run(std::move(suggestions_result));
            // Mark the `GetSuggestions` callback as invoked to unblock current
            // runloop.
            get_suggestions_called_future.SetValue();
          });

  // Expect ScrollToForm.
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 0));

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Wait for the tool to resolve target frame and fetch suggestions.
  EXPECT_TRUE(get_suggestions_called_future.Wait());

  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);

  // Run the selection with nil (dialog dismissed).
  [intervention_delegate() runCompletionWithSuggestion:nil];

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFormFillingDialogError);
}

// Test that if FillSuggestions returns an error, the tool execution fails.
TEST_F(AttemptFormFillingToolTest, Execute_FillSuggestionsError) {
  web::FakeWebState* web_state = CreateAndInsertWebState();

  optimization_guide::proto::AttemptFormFillingAction action = CreateAction();
  std::unique_ptr<AttemptFormFillingTool> tool = CreateTool(action, web_state);
  ASSERT_TRUE(tool);

  std::vector<autofill::ActorFormFillingRequest> suggestions_result = {
      CreateAddressFormFillingRequest(),
  };

  // Expect that `GetSuggestions` gets called with callback. Block the runloop
  // until the callback is invoked.
  base::test::TestFuture<void> get_suggestions_called_future;
  EXPECT_CALL(*mock_form_filling_service(),
              GetSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [&](autofill::AutofillClient& client,
              base::span<const autofill::ActorFormFillingService::FillRequest>
                  fill_requests,
              autofill::ActorFormFillingService::GetSuggestionsCallback
                  callback) {
            std::move(callback).Run(std::move(suggestions_result));
            // Mark the `GetSuggestions` callback as invoked to unblock current
            // runloop.
            get_suggestions_called_future.SetValue();
          });

  // Expect ScrollToForm.
  EXPECT_CALL(*mock_form_filling_service(), ScrollToForm(testing::_, 0));

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Wait for the tool to resolve target frame and fetch suggestions.
  EXPECT_TRUE(get_suggestions_called_future.Wait());

  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);

  // Expect FillForm to be called.
  EXPECT_CALL(*mock_form_filling_service(),
              FillForm(testing::_, 0, testing::_));

  // Expect FillSuggestions to be called.
  EXPECT_CALL(*mock_form_filling_service(),
              FillSuggestions(testing::_, testing::_, testing::_))
      .WillOnce(
          [](autofill::AutofillClient& client,
             base::span<const autofill::ActorFormFillingSelection>
                 chosen_suggestions,
             base::OnceCallback<void(
                 base::expected<std::string, autofill::ActorFormFillingError>)>
                 callback) {
            std::move(callback).Run(
                base::unexpected(autofill::ActorFormFillingError::kOther));
          });

  ActorFormSuggestion* selected_suggestion =
      intervention_delegate().promptedSuggestions[0];

  // Run the selection.
  [intervention_delegate() runCompletionWithSuggestion:selected_suggestion];

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(),
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
}

}  // namespace actor
