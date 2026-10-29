// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/coordinator/form_input_accessory_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_view_controller.h"
#import "ios/chrome/browser/autofill/form_input_accessory/public/autofill_suggestion_context_menu_handler.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Category exposing protocol conformance of `FormInputAccessoryCoordinator` for
// unit tests.
@interface FormInputAccessoryCoordinator (Testing) <
    AutofillAiSourcesViewControllerDelegate,
    AutofillSuggestionContextMenuHandler>
@end

namespace {

using autofill::EntityInstance;
using autofill::Suggestion;
using autofill::SuggestionType;
using GmailSourceMetadata =
    EntityInstance::PersonalContextRecordTypePayload::GmailSourceMetadata;
using PersonalContextRecordTypePayload =
    EntityInstance::PersonalContextRecordTypePayload;
using Source = EntityInstance::PersonalContextRecordTypePayload::Source;

constexpr char kPassportGuid[] = "00000000-0000-4000-8000-000000000001";
constexpr char kDriversLicenseGuid[] = "00000000-0000-4000-8000-000000000002";
constexpr char kNationalIdCardGuid[] = "00000000-0000-4000-8000-000000000003";
constexpr char kVehicleGuid[] = "00000000-0000-4000-8000-000000000004";
constexpr char kKnownTravelerGuid[] = "00000000-0000-4000-8000-000000000005";
constexpr char kRedressNumberGuid[] = "00000000-0000-4000-8000-000000000006";
constexpr char kFlightReservationGuid[] =
    "00000000-0000-4000-8000-000000000007";
constexpr char kOrderGuid[] = "00000000-0000-4000-8000-000000000008";
constexpr char kShipmentGuid[] = "00000000-0000-4000-8000-000000000009";
constexpr char kTestSourceUrl[] = "https://mail.google.com/mail/u/0/#inbox/1";

// Creates a `FormSuggestion` of `type` with an empty payload.
FormSuggestion* CreateSuggestion(SuggestionType type) {
  return [FormSuggestion suggestionWithValue:@"Test"
                          displayDescription:nil
                                        icon:nil
                                        type:type
                                     payload:Suggestion::Payload()
                              requiresReauth:NO];
}

// Creates an Autofill AI `FormSuggestion` referencing `guid`.
FormSuggestion* CreateAutofillAiSuggestion(std::string_view guid) {
  Suggestion::AutofillAiPayload payload(
      EntityInstance::EntityId{std::string(guid)});
  return [FormSuggestion suggestionWithValue:@"Test"
                          displayDescription:@"Subtitle"
                                        icon:nil
                                        type:SuggestionType::kFillAutofillAi
                                     payload:payload
                              requiresReauth:NO];
}

struct OpenSettingsTestCase {
  SuggestionType suggestion_type;
  std::string_view entity_guid;
  AutofillSettingsPage expected_page_when_your_saved_info_disabled;
  AutofillSettingsPage expected_page_when_your_saved_info_enabled;
};

constexpr OpenSettingsTestCase kOpenSettingsTestCases[] = {
    {SuggestionType::kPasswordEntry, "", AutofillSettingsPage::kPasswordManager,
     AutofillSettingsPage::kPasswordManager},
    {SuggestionType::kBackupPasswordEntry, "",
     AutofillSettingsPage::kPasswordManager,
     AutofillSettingsPage::kPasswordManager},
    {SuggestionType::kCreditCardEntry, "", AutofillSettingsPage::kCreditCards,
     AutofillSettingsPage::kCreditCards},
    {SuggestionType::kVirtualCreditCardEntry, "",
     AutofillSettingsPage::kCreditCards, AutofillSettingsPage::kCreditCards},
    {SuggestionType::kAddressEntry, "", AutofillSettingsPage::kAddresses,
     AutofillSettingsPage::kAddresses},
    {SuggestionType::kFillAutofillAi, kPassportGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kIdentityDocs},
    {SuggestionType::kFillAutofillAi, kDriversLicenseGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kIdentityDocs},
    {SuggestionType::kFillAutofillAi, kNationalIdCardGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kIdentityDocs},
    {SuggestionType::kFillAutofillAi, kVehicleGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kTravel},
    {SuggestionType::kFillAutofillAi, kKnownTravelerGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kTravel},
    {SuggestionType::kFillAutofillAi, kRedressNumberGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kTravel},
    {SuggestionType::kFillAutofillAi, kFlightReservationGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kTravel},
    {SuggestionType::kFillAutofillAi, kOrderGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kShopping},
    {SuggestionType::kFillAutofillAi, kShipmentGuid,
     AutofillSettingsPage::kAddresses, AutofillSettingsPage::kShopping},
};

}  // namespace

class FormInputAccessoryCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    mock_navigator_ =
        OCMStrictProtocolMock(@protocol(AutofillSettingsNavigator));
    mock_scene_handler_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[FormInputAccessoryCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    coordinator_.navigator = mock_navigator_;
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    base_view_controller_ = nil;
    PlatformTest::TearDown();
  }

  // Populates `EntityDataManager` with one test entity for each Autofill AI
  // entity type.
  void PopulateAllEntityTypes() {
    autofill::EntityDataManager* entity_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
    ASSERT_NE(entity_manager, nullptr);
    entity_manager->SetPersonalContextEntitiesForTesting({
        autofill::test::GetPassportEntityInstance(
            {.guid = kPassportGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetDriversLicenseEntityInstance(
            {.guid = kDriversLicenseGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetNationalIdCardEntityInstance(
            {.guid = kNationalIdCardGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetVehicleEntityInstance(
            {.guid = kVehicleGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetKnownTravelerNumberInstance(
            {.guid = kKnownTravelerGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetRedressNumberEntityInstance(
            {.guid = kRedressNumberGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetFlightReservationEntityInstance(
            {.guid = kFlightReservationGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetOrderEntityInstance(
            {.guid = kOrderGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
        autofill::test::GetShipmentEntityInstance(
            {.guid = kShipmentGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
    });
  }

  // Populates `EntityDataManager` with a personal context passport entity.
  void PopulatePersonalContextPassport() {
    autofill::EntityDataManager* entity_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
    ASSERT_NE(entity_manager, nullptr);
    entity_manager->SetPersonalContextEntitiesForTesting({
        autofill::test::GetPassportEntityInstance(
            {.guid = kPassportGuid,
             .record_type = EntityInstance::RecordType::kPersonalContext}),
    });
  }

  // Populates `EntityDataManager` with an order entity containing a source and
  // a shipment entity without sources.
  void PopulateEntitiesForSourceAttribution() {
    autofill::EntityDataManager* entity_manager =
        IOSAutofillEntityDataManagerFactory::GetForProfile(profile_.get());
    ASSERT_NE(entity_manager, nullptr);
    entity_manager->SetPersonalContextEntitiesForTesting({
        autofill::test::GetOrderEntityInstance(
            {.guid = kOrderGuid,
             .record_type =
                 PersonalContextRecordTypePayload{
                     .sources = {Source{
                         .url = GURL(kTestSourceUrl),
                         .metadata =
                             GmailSourceMetadata{.title =
                                                     "Order confirmation"}}}}}),
        autofill::test::GetShipmentEntityInstance(
            {.guid = kShipmentGuid,
             .record_type = PersonalContextRecordTypePayload{.sources = {}}}),
    });
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  id mock_navigator_;
  id mock_scene_handler_;
  FormInputAccessoryCoordinator* coordinator_;
};

// Parameterized fixture, kept separate from `FormInputAccessoryCoordinatorTest`
// because `TEST_F` and `TEST_P` cannot share the same test suite name.
class FormInputAccessoryCoordinatorParamTest
    : public FormInputAccessoryCoordinatorTest,
      public testing::WithParamInterface<
          std::tuple<OpenSettingsTestCase, bool>> {};

// Tests that `openSettingsForSuggestion:` redirects to the expected settings
// page when `kYourSavedInfoSettingsPageIos` is enabled or disabled.
TEST_P(FormInputAccessoryCoordinatorParamTest, OpenSettingsForSuggestion) {
  const auto& [test_case, your_saved_info_enabled] = GetParam();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kYourSavedInfoSettingsPageIos,
                                    your_saved_info_enabled);
  PopulateAllEntityTypes();

  FormSuggestion* suggestion =
      test_case.suggestion_type == SuggestionType::kFillAutofillAi
          ? CreateAutofillAiSuggestion(test_case.entity_guid)
          : CreateSuggestion(test_case.suggestion_type);
  AutofillSettingsPage expected_page =
      your_saved_info_enabled
          ? test_case.expected_page_when_your_saved_info_enabled
          : test_case.expected_page_when_your_saved_info_disabled;

  OCMExpect([mock_navigator_ openSettingsForPage:expected_page]);
  [coordinator_ openSettingsForSuggestion:suggestion];
  EXPECT_OCMOCK_VERIFY(mock_navigator_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FormInputAccessoryCoordinatorParamTest,
    testing::Combine(testing::ValuesIn(kOpenSettingsTestCases),
                     testing::Bool()));

// Tests that `isPersonalContextSuggestion:` returns YES only for a suggestion
// matching a stored personal context entity.
TEST_F(FormInputAccessoryCoordinatorTest, IsPersonalContextSuggestion) {
  PopulatePersonalContextPassport();

  FormSuggestion* personal_context_suggestion =
      CreateAutofillAiSuggestion(kPassportGuid);
  FormSuggestion* unknown_entity_suggestion =
      CreateAutofillAiSuggestion(kDriversLicenseGuid);

  EXPECT_TRUE(
      [coordinator_ isPersonalContextSuggestion:personal_context_suggestion]);
  EXPECT_FALSE(
      [coordinator_ isPersonalContextSuggestion:unknown_entity_suggestion]);
}

// Tests that `canSuppressPersonalContextSuggestion:` returns YES only for a
// suggestion matching a stored personal context entity when
// `kAutofillAmbientAutofillSuppression` is enabled.
TEST_F(FormInputAccessoryCoordinatorTest,
       CanSuppressPersonalContextSuggestion_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAmbientAutofillSuppression);
  PopulatePersonalContextPassport();

  FormSuggestion* personal_context_suggestion =
      CreateAutofillAiSuggestion(kPassportGuid);
  FormSuggestion* unknown_entity_suggestion =
      CreateAutofillAiSuggestion(kDriversLicenseGuid);

  EXPECT_TRUE([coordinator_
      canSuppressPersonalContextSuggestion:personal_context_suggestion]);
  EXPECT_FALSE([coordinator_
      canSuppressPersonalContextSuggestion:unknown_entity_suggestion]);
}

// Tests that `canSuppressPersonalContextSuggestion:` returns NO when
// `kAutofillAmbientAutofillSuppression` is disabled.
TEST_F(FormInputAccessoryCoordinatorTest,
       CanSuppressPersonalContextSuggestion_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAmbientAutofillSuppression);
  PopulatePersonalContextPassport();

  FormSuggestion* personal_context_suggestion =
      CreateAutofillAiSuggestion(kPassportGuid);

  EXPECT_FALSE([coordinator_
      canSuppressPersonalContextSuggestion:personal_context_suggestion]);
}

// Tests that `hasSourcesForSuggestion:` returns YES only for a personal context
// entity with non-empty sources when
// `kAutofillAmbientAutofillSourceAttribution` is enabled.
TEST_F(FormInputAccessoryCoordinatorTest,
       HasSourcesForSuggestion_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAmbientAutofillSourceAttribution);
  PopulateEntitiesForSourceAttribution();

  FormSuggestion* suggestion_with_sources =
      CreateAutofillAiSuggestion(kOrderGuid);
  FormSuggestion* suggestion_without_sources =
      CreateAutofillAiSuggestion(kShipmentGuid);

  EXPECT_TRUE([coordinator_ hasSourcesForSuggestion:suggestion_with_sources]);
  EXPECT_FALSE(
      [coordinator_ hasSourcesForSuggestion:suggestion_without_sources]);
}

// Tests that `hasSourcesForSuggestion:` returns NO when
// `kAutofillAmbientAutofillSourceAttribution` is disabled.
TEST_F(FormInputAccessoryCoordinatorTest,
       HasSourcesForSuggestion_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAmbientAutofillSourceAttribution);
  PopulateEntitiesForSourceAttribution();

  FormSuggestion* suggestion_with_sources =
      CreateAutofillAiSuggestion(kOrderGuid);

  EXPECT_FALSE([coordinator_ hasSourcesForSuggestion:suggestion_with_sources]);
}

// Tests that selecting a source item in
// `AutofillAiSourcesViewControllerDelegate` opens the source URL in a new tab.
TEST_F(FormInputAccessoryCoordinatorTest, OpenSourceItem) {
  AutofillAiSourceItem* source_item =
      [[AutofillAiSourceItem alloc] initWithTitle:@"Order confirmation"
                                         subtitle:nil
                                              URL:GURL(kTestSourceUrl)
                                             type:AutofillAiSourceType::kGmail
                                             icon:nil];

  OCMExpect([mock_scene_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.URL == GURL(kTestSourceUrl);
      }]]);

  [coordinator_ sourcesViewController:nil didSelectSourceItem:source_item];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}
