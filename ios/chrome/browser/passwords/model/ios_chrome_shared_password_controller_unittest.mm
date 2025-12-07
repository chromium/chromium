// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_shared_password_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_utils.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::SuggestionType;

namespace {

constexpr NSString* kSuggestionDisplayDescription = @"displayDescription";
constexpr NSString* kSuggestionValue = @"value";
constexpr NSString* kTestFrameID = @"frameID";
constexpr std::string kTestURL = "https://www.test.com/";

// Creates a generic FormSuggestionProviderQuery.
FormSuggestionProviderQuery* CreateFormSuggestionProviderQuery() {
  return [[FormSuggestionProviderQuery alloc]
      initWithFormName:@"formName"
        formRendererID:autofill::test::MakeFormRendererId()
       fieldIdentifier:@"fieldIdentifier"
       fieldRendererID:autofill::test::MakeFieldRendererId()
             fieldType:kObfuscatedFieldType  // Ensures this is a password form.
                  type:@"focus"
            typedValue:@""
               frameID:kTestFrameID
          onlyPassword:YES];
}

// Creates a password form suggestion. The created suggestion will be a backup
// or normal password suggestion depenging on `is_backup_password`.
FormSuggestion* CreatePasswordFormSuggestion(bool is_backup_password = false,
                                             UIImage* icon = nil) {
  SuggestionType suggestion_type = is_backup_password
                                       ? SuggestionType::kBackupPasswordEntry
                                       : SuggestionType::kPasswordEntry;
  return [FormSuggestion suggestionWithValue:kSuggestionValue
                          displayDescription:kSuggestionDisplayDescription
                                        icon:icon
                                        type:suggestion_type
                                     payload:autofill::Suggestion::Payload()
                              requiresReauth:YES];
}

// Verifies that the `suggestion`'s fields match that of the
// `expected_suggestion`.
void VerifyPasswordFormSuggestion(FormSuggestion* suggestion,
                                  FormSuggestion* expected_suggestion) {
  EXPECT_NSEQ(suggestion.value, expected_suggestion.value);
  EXPECT_NSEQ(suggestion.minorValue, expected_suggestion.minorValue);
  EXPECT_NSEQ(suggestion.displayDescription,
              expected_suggestion.displayDescription);
  EXPECT_NSEQ(suggestion.icon, expected_suggestion.icon);
  EXPECT_EQ(suggestion.type, expected_suggestion.type);
  EXPECT_EQ(suggestion.fieldByFieldFillingTypeUsed,
            expected_suggestion.fieldByFieldFillingTypeUsed);
  EXPECT_EQ(suggestion.requiresReauth, expected_suggestion.requiresReauth);
  EXPECT_NSEQ(suggestion.acceptanceA11yAnnouncement,
              expected_suggestion.acceptanceA11yAnnouncement);
  EXPECT_EQ(suggestion.metadata.is_single_username_form,
            expected_suggestion.metadata.is_single_username_form);
  EXPECT_EQ(suggestion.metadata.likely_from_real_password_field,
            expected_suggestion.metadata.likely_from_real_password_field);
  EXPECT_EQ(suggestion.params, expected_suggestion.params);
  EXPECT_EQ(suggestion.provider, expected_suggestion.provider);
  EXPECT_EQ(suggestion.featureForIPH, expected_suggestion.featureForIPH);
  EXPECT_EQ(suggestion.suggestionIconType,
            expected_suggestion.suggestionIconType);
}

}  // namespace

// Test fixture for IOSChromeSharedPasswordController.
class IOSChromeSharedPasswordControllerTest : public PlatformTest {
 public:
  IOSChromeSharedPasswordControllerTest() : PlatformTest() {
    // Set up the web state.
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    fake_web_state_.SetWebFramesManager(content_world,
                                        std::move(web_frames_manager));
    fake_web_state_.SetCurrentURL(GURL(kTestURL));

    // Set up the frame.
    auto web_frame =
        web::FakeWebFrame::Create(base::SysNSStringToUTF8(kTestFrameID),
                                  /*is_main_frame=*/false, GURL(kTestURL));
    frame_ = web_frame.get();
    web_frames_manager_->AddWebFrame(std::move(web_frame));

    suggestion_helper_ = OCMClassMock([PasswordSuggestionHelper class]);

    controller_ = [[IOSChromeSharedPasswordController alloc]
        initWithWebState:&fake_web_state_
                 manager:nil
              formHelper:nil
        suggestionHelper:suggestion_helper_
            driverHelper:nil];

    delegate_ = OCMProtocolMock(@protocol(SharedPasswordControllerDelegate));
    password_manager::PasswordManagerClient* client_ptr =
        &password_manager_client_;
    [[[delegate_ stub] andReturnValue:OCMOCK_VALUE(client_ptr)]
        passwordManagerClient];
    controller_.delegate = delegate_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<web::FakeWebFramesManager, DanglingUntriaged> web_frames_manager_;
  web::FakeWebState fake_web_state_;
  raw_ptr<web::WebFrame> frame_;
  id suggestion_helper_;
  id delegate_;
  password_manager::StubPasswordManagerClient password_manager_client_;
  SharedPasswordController* controller_;
};

// Tests that both regular and backup password suggestions are as expected when
// retrieved.
TEST_F(IOSChromeSharedPasswordControllerTest, ReturnsSuggestionsIfAvailable) {
  FormSuggestionProviderQuery* form_query = CreateFormSuggestionProviderQuery();

  FormSuggestion* password_suggestion = CreatePasswordFormSuggestion();
  FormSuggestion* backup_password_suggestion =
      CreatePasswordFormSuggestion(/*is_backup_password=*/true);
  NSArray<FormSuggestion*>* suggestions =
      @[ password_suggestion, backup_password_suggestion ];
  OCMExpect([suggestion_helper_ retrieveSuggestionsWithForm:form_query])
      .andReturn(suggestions);

  FormSuggestion* expected_backup_password_suggestion =
      CreatePasswordFormSuggestion(/*is_backup_password=*/true,
                                   /*icon=*/GetBackupPasswordSuggestionIcon());
  expected_backup_password_suggestion.suggestionIconType =
      SuggestionIconType::kBackupPassword;

  __block BOOL completion_was_called = NO;
  [controller_ retrieveSuggestionsForForm:form_query
                                 webState:&fake_web_state_
                        completionHandler:^(
                            NSArray<FormSuggestion*>* retrieved_suggestions,
                            id<FormSuggestionProvider> delegate) {
                          EXPECT_EQ(2u, retrieved_suggestions.count);

                          VerifyPasswordFormSuggestion(retrieved_suggestions[0],
                                                       password_suggestion);
                          VerifyPasswordFormSuggestion(
                              retrieved_suggestions[1],
                              expected_backup_password_suggestion);
                          completion_was_called = YES;
                        }];

  EXPECT_TRUE(completion_was_called);

  EXPECT_OCMOCK_VERIFY(suggestion_helper_);
}
