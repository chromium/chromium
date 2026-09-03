// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
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

    BrowserAutofillManager* autofill_manager =
        static_cast<BrowserAutofillManager*>(
            autofill_client_->GetAutofillManagerForPrimaryMainFrame());
    mediator_ = [[AtMemoryMediator alloc]
        initWithAtMemoryManager:at_memory_manager_.get()
                autofillManager:autofill_manager
                contentInjector:mock_injector_
                        fieldId:FieldGlobalId()];
    mediator_.atMemoryHandler = mock_at_memory_handler_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    mock_injector_ = nil;
    at_memory_manager_.reset();
    autofill_client_.reset();
    PlatformTest::TearDown();
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

// Tests that fillWithContent: forwards to the content injector.
TEST_F(AtMemoryMediatorTest, FillWithContentCallsInjector) {
  OCMExpect([mock_injector_
      userDidPickContent:kTestContent
           passwordField:NO
           requiresHTTPS:YES
         jumpToNextField:NO
              actionType:autofill::mojom::FieldActionType::
                             kReplaceSelectionForAtMemory]);

  [mediator_ fillWithContent:kTestContent];

  EXPECT_OCMOCK_VERIFY(mock_injector_);
}

// Tests that fillWithSuggestion: with a non-obfuscated suggestion uses simple
// filling via the content injector.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionNonObfuscatedFillsValue) {
  Suggestion suggestion(base::SysNSStringToUTF16(kTestContent),
                        SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(base::SysNSStringToUTF16(kTestContent),
                                      MemoryDataType::kNameFull);
  suggestion.payload = std::move(payload);

  OCMExpect([mock_injector_
      userDidPickContent:kTestContent
           passwordField:NO
           requiresHTTPS:YES
         jumpToNextField:NO
              actionType:autofill::mojom::FieldActionType::
                             kReplaceSelectionForAtMemory]);

  [mediator_ fillWithSuggestion:suggestion];

  EXPECT_OCMOCK_VERIFY(mock_injector_);
}

// Tests that fillWithSuggestion: with an obfuscated suggestion delegates to
// AtMemoryManager without simple injection.
TEST_F(AtMemoryMediatorTest, FillWithSuggestionObfuscatedFills) {
  Suggestion suggestion(base::SysNSStringToUTF16(kObfuscatedContent),
                        SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(
      base::SysNSStringToUTF16(kObfuscatedContent),
      MemoryDataType::kPassportNumber);
  payload.is_personal_context_sourced = true;
  suggestion.payload = std::move(payload);

  [[mock_injector_ reject] userDidPickContent:[OCMArg any]
                                passwordField:NO
                                requiresHTTPS:YES
                              jumpToNextField:NO
                                   actionType:autofill::mojom::FieldActionType::
                                                  kReplaceSelectionForAtMemory];

  [mediator_ fillWithSuggestion:suggestion];

  EXPECT_OCMOCK_VERIFY(mock_injector_);
}
