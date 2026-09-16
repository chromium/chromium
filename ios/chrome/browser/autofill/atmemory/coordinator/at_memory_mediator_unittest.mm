// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/unguessable_token.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using autofill::AtMemoryManager;
using autofill::BrowserAutofillManager;
using autofill::FieldGlobalId;
using autofill::MemoryDataType;
using autofill::Suggestion;
using autofill::SuggestionType;
using autofill::TestAutofillClientIOS;

namespace {

NSString* const kTestContent = @"John Doe";
NSString* const kObfuscatedContent = @"AA123456";

// Returns a valid FieldGlobalId for testing.
FieldGlobalId CreateTestFieldGlobalId() {
  return FieldGlobalId(
      autofill::LocalFrameToken(base::UnguessableToken::Create()),
      autofill::FieldRendererId(42));
}

// Returns a standard non-obfuscated AtMemory suggestion for testing.
Suggestion CreateTestSuggestion() {
  Suggestion suggestion(base::SysNSStringToUTF16(kTestContent),
                        SuggestionType::kAtMemorySearchResult);
  suggestion.payload = Suggestion::AtMemoryPayload(
      base::SysNSStringToUTF16(kTestContent), MemoryDataType::kNameFull);
  return suggestion;
}

// Returns an obfuscated AtMemory suggestion for testing.
Suggestion CreateObfuscatedSuggestion() {
  Suggestion suggestion(base::SysNSStringToUTF16(kObfuscatedContent),
                        SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kObfuscatedContent),
      MemoryDataType::kPassportNumber);
  payload.is_personal_context_sourced = true;
  suggestion.payload = std::move(payload);
  return suggestion;
}

}  // namespace

// Test fixture for AtMemoryMediator.
class AtMemoryMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::kAutofillAtMemory,
                              autofill::features::debug::
                                  kAtMemorySkipEnablementChecks},
        /*disabled_features=*/{});

    web_state_.SetVisibleURL(GURL("http://example.org"));

    auto fake_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL("http://example.org"));
    fake_frames_manager->AddWebFrame(std::move(main_frame));
    web_state_.SetWebFramesManager(
        autofill::AutofillJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::move(fake_frames_manager));

    autofill_client_ =
        std::make_unique<TestAutofillClientIOS>(&web_state_, nil);
    at_memory_manager_ = std::make_unique<AtMemoryManager>(
        autofill_client_.get(), /*history_service=*/nullptr);

    mock_injector_ = OCMProtocolMock(@protocol(ManualFillContentInjector));
    mock_at_memory_handler_ = OCMProtocolMock(@protocol(AtMemoryCommands));

    mediator_ = CreateMediatorWithFieldId(FieldGlobalId());
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_injector_ = nil;
    at_memory_manager_.reset();
    autofill_client_.reset();
    PlatformTest::TearDown();
  }

  // Creates and configures an AtMemoryMediator with the specified `field_id`.
  AtMemoryMediator* CreateMediatorWithFieldId(FieldGlobalId field_id) {
    BrowserAutofillManager* autofill_manager =
        static_cast<BrowserAutofillManager*>(
            autofill_client_->GetAutofillManagerForPrimaryMainFrame());
    AtMemoryMediator* mediator = [[AtMemoryMediator alloc]
        initWithAtMemoryManager:at_memory_manager_.get()
                autofillManager:autofill_manager
                contentInjector:mock_injector_
                        fieldId:field_id];
    mediator.atMemoryHandler = mock_at_memory_handler_;
    return mediator;
  }

  // Sets OCMock expectations for filling `content` and dismissing AtMemory.
  void ExpectSuccessfulFillAndDismiss(NSString* content) {
    OCMExpect([mock_injector_
        userDidPickContent:content
             passwordField:NO
             requiresHTTPS:YES
           jumpToNextField:NO
                actionType:autofill::mojom::FieldActionType::
                               kReplaceSelectionForAtMemory]);
    OCMExpect([mock_at_memory_handler_ dismissAtMemory]);
  }

  // Asserts that no content is injected, i.e. that the fill is performed
  // exclusively through `AtMemoryManager`.
  void RejectContentInjection() {
    [[mock_injector_ reject]
        userDidPickContent:[OCMArg any]
             passwordField:NO
             requiresHTTPS:YES
           jumpToNextField:NO
                actionType:autofill::mojom::FieldActionType::
                               kReplaceSelectionForAtMemory];
  }

  // Verifies all OCMock expectations for injector and AtMemory handler.
  void VerifyMocks() {
    EXPECT_OCMOCK_VERIFY(mock_injector_);
    EXPECT_OCMOCK_VERIFY(mock_at_memory_handler_);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
  std::unique_ptr<TestAutofillClientIOS> autofill_client_;
  std::unique_ptr<AtMemoryManager> at_memory_manager_;
  id mock_injector_;
  id mock_at_memory_handler_;
  AtMemoryMediator* mediator_;
};

// Tests that fillWithContent: forwards to the content injector and dismisses
// AtMemory.
TEST_F(AtMemoryMediatorTest, FillWithContentCallsInjectorAndDismisses) {
  ExpectSuccessfulFillAndDismiss(kTestContent);

  [mediator_ fillWithContent:kTestContent];

  VerifyMocks();
}

// Tests that fillWithSuggestion: with a non-obfuscated suggestion fills via
// AtMemoryManager only, without also injecting the value a second time.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionNonObfuscatedFillsValue) {
  RejectContentInjection();
  OCMExpect([mock_at_memory_handler_ dismissAtMemory]);

  [mediator_ fillWithSuggestion:CreateTestSuggestion()];

  VerifyMocks();
}

// Tests that fillWithSuggestion: with an obfuscated suggestion delegates to
// AtMemoryManager and dismisses AtMemory directly without simple injection.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionObfuscatedFillsAndDismisses) {
  RejectContentInjection();
  OCMExpect([mock_at_memory_handler_ dismissAtMemory]);

  [mediator_ fillWithSuggestion:CreateObfuscatedSuggestion()];

  VerifyMocks();
}

// Tests that fillWithSuggestion: with an obfuscated suggestion succeeds when
// initialized with a valid FieldGlobalId.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionObfuscatedWithValidFieldId) {
  AtMemoryMediator* mediator =
      CreateMediatorWithFieldId(CreateTestFieldGlobalId());

  RejectContentInjection();

  [mediator fillWithSuggestion:CreateObfuscatedSuggestion()];

  EXPECT_OCMOCK_VERIFY(mock_injector_);
  [mediator disconnect];
}

// Tests that fillWithSuggestion: records the suggestion in AtMemoryManager
// when kAutofillAtMemorySearchStatefulness is enabled.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionRecordsInAtMemoryManager) {
  base::test::ScopedFeatureList statefulness_feature;
  statefulness_feature.InitWithFeatures(
      /*enabled_features=*/
      {autofill::features::kAutofillAtMemorySearchStatefulness,
       autofill::features::kAutofillAtMemoryPreviouslyFilled},
      /*disabled_features=*/{});

  FieldGlobalId field_id = CreateTestFieldGlobalId();
  AtMemoryMediator* mediator = CreateMediatorWithFieldId(field_id);

  RejectContentInjection();
  OCMExpect([mock_at_memory_handler_ dismissAtMemory]);

  at_memory_manager_->GetStateForField(field_id, url::Origin());
  [mediator fillWithSuggestion:CreateTestSuggestion()];

  VerifyMocks();

  std::vector<Suggestion> empty_query_suggestions =
      at_memory_manager_->GetEmptyQuerySuggestions();
  ASSERT_GE(empty_query_suggestions.size(), 2u);
  EXPECT_EQ(empty_query_suggestions[0].type, SuggestionType::kTitle);
  EXPECT_EQ(empty_query_suggestions[1].type,
            SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(empty_query_suggestions[1].main_text.value,
            base::SysNSStringToUTF16(kTestContent));

  [mediator disconnect];
}
