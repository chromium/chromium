// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/password_manager/ios/test_helpers.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_presenter.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr char kTestUrl[] = "http://foo.com";
constexpr char kFillDataUsername[] = "donut.guy@gmail.com";
constexpr char kFillDataPassword[] = "super!secret";
constexpr char kMainFrameId[] = "frameID";
constexpr autofill::FormRendererId kFormRendererId(1);
constexpr autofill::FieldRendererId kUsernameFieldRendererId(2);
constexpr autofill::FieldRendererId kPasswordFieldRendererId(3);

// Creates suggestion for a single username form.
FormSuggestion* SuggestionForSingleUsernameForm() {
  return [FormSuggestion
             suggestionWithValue:@"foo"
              displayDescription:nil
                            icon:nil
                            type:autofill::SuggestionType::kAutocompleteEntry
                         payload:autofill::Suggestion::Payload()
                  requiresReauth:NO
      acceptanceA11yAnnouncement:nil
                        metadata:{.is_single_username_form = true}];
}

// Gets the primary action label localized string for password fill.
NSString* PrimaryActionLabelForPasswordFill() {
  return l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD);
}

// Gets the primary action label localized string for password fill.
NSString* PrimaryActionLabelForUsernameFill() {
  return l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE);
}

// Creates PasswordFormFillData to be processed for offering password
// suggestions.
autofill::PasswordFormFillData CreatePasswordFillData(
    autofill::FormRendererId form_renderer_id,
    autofill::FieldRendererId username_renderer_id,
    autofill::FieldRendererId password_renderer_id) {
  autofill::PasswordFormFillData form_fill_data;
  test_helpers::SetPasswordFormFillData(
      kTestUrl, "", form_renderer_id.value(), "", username_renderer_id.value(),
      kFillDataUsername, "", password_renderer_id.value(), kFillDataPassword,
      nullptr, nullptr, &form_fill_data);
  return form_fill_data;
}

}  // namespace

// Expose the internal disconnect function for testing purposes
@interface CredentialSuggestionBottomSheetMediator ()

- (void)disconnect;

@end

// Test provider that records invocations of its interface methods.
@interface CredentialSuggestionBottomSheetMediatorTestSuggestionProvider
    : NSObject <FormSuggestionProvider>

@property(weak, nonatomic, readonly) FormSuggestion* suggestion;
@property(nonatomic, assign) NSInteger index;
@property(weak, nonatomic, readonly) NSString* formName;
@property(weak, nonatomic, readonly) NSString* fieldIdentifier;
@property(weak, nonatomic, readonly) NSString* frameID;
@property(nonatomic, assign) BOOL selected;
@property(nonatomic, assign) BOOL askedIfSuggestionsAvailable;
@property(nonatomic, assign) BOOL askedForSuggestions;
@property(nonatomic, assign) SuggestionProviderType type;
@property(nonatomic, readonly) autofill::FillingProduct mainFillingProduct;

// YES if the suggestion provider is used to provide suggestions on a single
// username form.
@property(nonatomic, readonly) BOOL forSingleUsernameForm;

// Creates a test provider with default suggestions.
+ (instancetype)providerWithSuggestions;

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions;

@end

@implementation CredentialSuggestionBottomSheetMediatorTestSuggestionProvider {
  NSArray<FormSuggestion*>* _suggestions;
  NSString* _formName;
  autofill::FormRendererId _formRendererID;
  NSString* _fieldIdentifier;
  autofill::FieldRendererId _fieldRendererID;
  NSString* _frameID;
  FormSuggestion* _suggestion;
}

@synthesize selected = _selected;
@synthesize askedIfSuggestionsAvailable = _askedIfSuggestionsAvailable;
@synthesize askedForSuggestions = _askedForSuggestions;

+ (instancetype)providerWithSuggestions {
  NSArray<FormSuggestion*>* suggestions = @[
    [FormSuggestion
        suggestionWithValue:@"foo"
         displayDescription:nil
                       icon:nil
                       type:autofill::SuggestionType::kAutocompleteEntry
                    payload:autofill::Suggestion::Payload()
             requiresReauth:NO],
    [FormSuggestion suggestionWithValue:@"bar"
                     displayDescription:nil
                                   icon:nil
                                   type:autofill::SuggestionType::kAddressEntry
                                payload:autofill::Suggestion::Payload()
                         requiresReauth:NO]
  ];
  return [[CredentialSuggestionBottomSheetMediatorTestSuggestionProvider alloc]
      initWithSuggestions:suggestions];
}

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  self = [super init];
  if (self) {
    _suggestions = [suggestions copy];
    _type = SuggestionProviderTypeUnknown;
    // Detect whether the suggestions were set up for a single username form,
    // based on the content of the suggestions.
    for (FormSuggestion* suggestion in _suggestions) {
      _forSingleUsernameForm =
          _forSingleUsernameForm || suggestion.metadata.is_single_username_form;
    }
  }
  return self;
}

- (NSString*)formName {
  return _formName;
}

- (NSString*)fieldIdentifier {
  return _fieldIdentifier;
}

- (NSString*)frameID {
  return _frameID;
}

- (FormSuggestion*)suggestion {
  return _suggestion;
}

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  self.askedIfSuggestionsAvailable = YES;
  completion([_suggestions count] > 0);
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  self.askedForSuggestions = YES;
  completion(_suggestions, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(autofill::FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  self.selected = YES;
  _suggestion = suggestion;
  _index = index;
  _formName = [formName copy];
  _formRendererID = formRendererID;
  _fieldIdentifier = [fieldIdentifier copy];
  _fieldRendererID = fieldRendererID;
  _frameID = [frameID copy];
  completion();
}

@end

class CredentialSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  CredentialSuggestionBottomSheetMediatorTest()
      : web_state_(std::make_unique<web::FakeWebState>()),
        web_state_ptr_(web_state_.get()) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
  }

  void SetUp() override {
    web_state_->SetCurrentURL(GURL(kTestUrl));

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs_ptr_ = prefs.get();
    RegisterProfilePrefs(prefs_ptr_->registry());

    // Set up the Profile used by the webstate.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              ios::FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(&password_manager::BuildPasswordStore<
                       ProfileIOS, password_manager::TestPasswordStore>));
    builder.SetPrefService(std::move(prefs));
    profile_ = std::move(builder).Build();

    web_state_->SetBrowserState(profile_.get());

    // Set up the javascript feature manager for the profile so no-op JS calls
    // can be made on the fake frame.
    web::test::OverrideJavaScriptFeatures(
        profile_.get(),
        {password_manager::PasswordManagerJavaScriptFeature::GetInstance()});

    // Set up the frames manager so frames can be used.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager_ptr_ = frames_manager.get();
    web_state_->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                    std::move(frames_manager));

    // Create the PasswordTabHelper so the credential suggestion provider is
    // available.
    PasswordTabHelper::CreateForWebState(web_state_.get());

    consumer_ =
        OCMProtocolMock(@protocol(CredentialSuggestionBottomSheetConsumer));
    presenter_ = OCMStrictProtocolMock(
        @protocol(CredentialSuggestionBottomSheetPresenter));

    params_.frame_id = kMainFrameId;
    params_.form_name = "form";
    params_.form_renderer_id = kFormRendererId;
    params_.field_renderer_id = kUsernameFieldRendererId;
    params_.field_identifier = "field_id";
    params_.field_type = "select-one";
    params_.type = "type";
    // Set the value to be empty so all the suggestions can be offered without
    // doing any filtering based on prefix matching.
    params_.value = "";
    params_.input_missing = false;

    suggestion_providers_ = @[];
  }

  void TearDown() override {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(presenter_);
  }

  void CreateMediator() {
    // Create the FormSuggestionTabHelper with test providers used by the
    // credential bottom sheet v1.
    FormSuggestionTabHelper::CreateForWebState(web_state_.get(),
                                               suggestion_providers_);

    web_state_list_->InsertWebState(
        std::move(web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

    // Create a frame so credential suggestions can be provided for that frame.
    auto main_frame = web::FakeWebFrame::Create(
        kMainFrameId, /*is_main_frame=*/true, GURL(kTestUrl));
    main_frame_ptr_ = main_frame.get();
    main_frame_ptr_->set_browser_state(profile_.get());
    frames_manager_ptr_->AddWebFrame(std::move(main_frame));

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    // Set up the fill data for the real credential suggestion provider.
    if ([suggestion_providers_ count] > 0) {
      ASSERT_EQ(1u, [suggestion_providers_ count]);
      CredentialSuggestionBottomSheetMediatorTestSuggestionProvider* provider =
          base::apple::ObjCCastStrict<
              CredentialSuggestionBottomSheetMediatorTestSuggestionProvider>(
              [suggestion_providers_ objectAtIndex:0]);
      SetUpFillDataInPasswordManager(provider.forSingleUsernameForm);
    }

    mediator_ = [[CredentialSuggestionBottomSheetMediator alloc]
          initWithWebStateList:web_state_list_.get()
                 faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                   profile_.get())
                   prefService:prefs_ptr_
                        params:params_
                  reauthModule:nil
                           URL:URL()
          profilePasswordStore:store_
          accountPasswordStore:nullptr
        sharedURLLoaderFactory:nullptr
             engagementTracker:nil
                     presenter:nil];

    // Run the queued JS feature callback.
    base::RunLoop().RunUntilIdle();
  }

  // Creates the bottom sheet mediator with custom suggestions `providers`.
  void CreateMediatorWithSuggestions(
      NSArray<id<FormSuggestionProvider>>* providers) {
    suggestion_providers_ = providers;
    CreateMediator();
  }

  // Creates the bottom sheet mediator with the default suggestions providers.
  void CreateMediatorWithDefaultSuggestions() {
    CreateMediatorWithSuggestions(
        @[ [CredentialSuggestionBottomSheetMediatorTestSuggestionProvider
            providerWithSuggestions] ]);
  }

  void SetUpFillDataInPasswordManager(bool for_single_username_form) {
    SharedPasswordController* shared_password_controller =
        PasswordTabHelper::FromWebState(web_state_ptr_)
            ->GetSharedPasswordController();

    // Set up the fill data based on whether or not the form is a single
    // username form. Single username forms do not have a renderer id for their
    // password field.
    autofill::PasswordFormFillData fill_data =
        for_single_username_form
            ? CreatePasswordFillData(kFormRendererId, kUsernameFieldRendererId,
                                     autofill::FieldRendererId(0))
            : CreatePasswordFillData(kFormRendererId, kUsernameFieldRendererId,
                                     kPasswordFieldRendererId);

    [shared_password_controller
        processPasswordFormFillData:fill_data
                         forFrameId:kMainFrameId
                        isMainFrame:YES
                  forSecurityOrigin:main_frame_ptr_->GetSecurityOrigin()];
  }

  GURL URL() { return GURL("http://foo.com"); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_ptr_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::WebState, DanglingUntriaged> web_state_ptr_;
  raw_ptr<web::FakeWebFramesManager, DanglingUntriaged> frames_manager_ptr_;
  raw_ptr<web::FakeWebFrame, DanglingUntriaged> main_frame_ptr_;
  scoped_refptr<password_manager::TestPasswordStore> store_;
  id consumer_;
  NSArray<id<FormSuggestionProvider>>* suggestion_providers_;
  autofill::FormActivityParams params_;
  CredentialSuggestionBottomSheetMediator* mediator_;
  id presenter_;
};

// Tests CredentialSuggestionBottomSheetMediator can be initialized.
TEST_F(CredentialSuggestionBottomSheetMediatorTest, Init) {
  CreateMediator();
  EXPECT_TRUE(mediator_);
}

// Tests consumer when suggestions are available.
TEST_F(CredentialSuggestionBottomSheetMediatorTest, WithSuggestions) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);

  OCMExpect([consumer_ setSuggestions:[OCMArg isNotNil]
                            andDomain:[OCMArg isNotNil]]);
  OCMExpect(
      [consumer_ setPrimaryActionString:PrimaryActionLabelForPasswordFill()]);

  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests setting the consumer when suggestions are available for a single
// username form and the feature is enabled.
TEST_F(CredentialSuggestionBottomSheetMediatorTest,
       WithSuggestions_ForSingleUsernameForm) {
  id<FormSuggestionProvider> provider =
      [[CredentialSuggestionBottomSheetMediatorTestSuggestionProvider alloc]
          initWithSuggestions:@[ SuggestionForSingleUsernameForm() ]];

  CreateMediatorWithSuggestions(@[ provider ]);
  ASSERT_TRUE(mediator_);

  OCMExpect([consumer_ setSuggestions:[OCMArg isNotNil]
                            andDomain:[OCMArg isNotNil]]);
  [[consumer_ expect]
      setPrimaryActionString:PrimaryActionLabelForUsernameFill()];

  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(CredentialSuggestionBottomSheetMediatorTest, IncrementDismissCount) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);

  EXPECT_EQ(prefs_ptr_->GetInteger(prefs::kIosPasswordBottomSheetDismissCount),
            0);
  [mediator_ onDismissWithoutAnyCredentialAction];
  EXPECT_EQ(prefs_ptr_->GetInteger(prefs::kIosPasswordBottomSheetDismissCount),
            1);
  [mediator_ onDismissWithoutAnyCredentialAction];
  EXPECT_EQ(prefs_ptr_->GetInteger(prefs::kIosPasswordBottomSheetDismissCount),
            2);
  [mediator_ onDismissWithoutAnyCredentialAction];
  EXPECT_EQ(prefs_ptr_->GetInteger(prefs::kIosPasswordBottomSheetDismissCount),
            3);

  // Expect failure after 3 times.
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_DEATH([mediator_ onDismissWithoutAnyCredentialAction],
               "Failed when dismiss count is incremented higher than the "
               "expected value.");
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(CredentialSuggestionBottomSheetMediatorTest,
       SuggestionUsernameHasSuffix) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);

  password_manager::CredentialUIEntry expectedCredential;
  expectedCredential.username = u"test1";
  expectedCredential.password = u"test1password";
  password_manager::CredentialFacet facet;
  GURL URL(u"http://www.example.com/");
  facet.signon_realm = URL.spec();
  expectedCredential.facets = {facet};
  [mediator_ setCredentialsForTesting:{expectedCredential}];

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test1"
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  std::optional<password_manager::CredentialUIEntry> credential =
      [mediator_ getCredentialForFormSuggestion:suggestion];
  EXPECT_TRUE(credential.has_value());
  EXPECT_EQ(credential.value(), expectedCredential);
}

TEST_F(CredentialSuggestionBottomSheetMediatorTest,
       SuggestionUsernameWithoutSuffix) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);

  password_manager::CredentialUIEntry expectedCredential;
  expectedCredential.username = u"test1";
  expectedCredential.password = u"test1password";
  password_manager::CredentialFacet facet;
  GURL URL(u"http://www.example.com/");
  facet.signon_realm = URL.spec();
  expectedCredential.facets = {facet};
  [mediator_ setCredentialsForTesting:{expectedCredential}];

  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"test1"
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
                  payload:autofill::Suggestion::Payload()
           requiresReauth:NO];
  std::optional<password_manager::CredentialUIEntry> credential =
      [mediator_ getCredentialForFormSuggestion:suggestion];
  EXPECT_TRUE(credential.has_value());
  EXPECT_EQ(credential.value(), expectedCredential);
}

// Tests that the mediator is correctly cleaned up when the WebStateList is
// destroyed. There are a lot of checked observer lists that could potentially
// cause a crash in the process, so this test ensures they're executed.
TEST_F(CredentialSuggestionBottomSheetMediatorTest,
       CleansUpWhenWebStateListDestroyed) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);
  [mediator_ setConsumer:consumer_];

  web_state_list_.reset();
  EXPECT_OCMOCK_VERIFY(consumer_);
}
