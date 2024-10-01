// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Creates suggestion for a single username form.
FormSuggestion* SuggestionForSingleUsernameForm() {
  return [FormSuggestion
             suggestionWithValue:@"foo"
              displayDescription:nil
                            icon:nil
                            type:autofill::SuggestionType::kAutocompleteEntry
               backendIdentifier:nil
                  requiresReauth:NO
      acceptanceA11yAnnouncement:nil
                        metadata:{.is_single_username_form = true}];
}

// Gets the primary action label localized string for password fill.
NSString* PrimaryActionLabelForPasswordFill() {
  return l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD);
}

// Gets the primary action label localized string for password fill.
NSString* PrimaryActionLabelForUsernameFill() {
  return l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_CONTINUE);
}

}  // namespace

// Expose the internal disconnect function for testing purposes
@interface PasswordSuggestionBottomSheetMediator ()

- (void)disconnect;

@end

// Test provider that records invocations of its interface methods.
@interface PasswordSuggestionBottomSheetMediatorTestSuggestionProvider
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

// Creates a test provider with default suggesstions.
+ (instancetype)providerWithSuggestions;

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions;

@end

@implementation PasswordSuggestionBottomSheetMediatorTestSuggestionProvider {
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
          backendIdentifier:nil
             requiresReauth:NO],
    [FormSuggestion suggestionWithValue:@"bar"
                     displayDescription:nil
                                   icon:nil
                                   type:autofill::SuggestionType::kAddressEntry
                      backendIdentifier:nil
                         requiresReauth:NO]
  ];
  return [[PasswordSuggestionBottomSheetMediatorTestSuggestionProvider alloc]
      initWithSuggestions:suggestions];
}

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  self = [super init];
  if (self) {
    _suggestions = [suggestions copy];
    _type = SuggestionProviderTypeUnknown;
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

class PasswordSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PasswordSuggestionBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()),
        profile_(TestProfileIOS::Builder().Build()) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
  }

  void SetUp() override {
    test_web_state_->SetCurrentURL(URL());

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
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    profile_ = std::move(builder).Build();

    consumer_ =
        OCMProtocolMock(@protocol(PasswordSuggestionBottomSheetConsumer));

    params_.form_name = "form";
    params_.field_identifier = "field_id";
    params_.field_type = "select-one";
    params_.type = "type";
    params_.value = "value";
    params_.input_missing = false;

    suggestion_providers_ = @[];
  }

  void TearDown() override { [mediator_ disconnect]; }

  void CreateMediator() {
    FormSuggestionTabHelper::CreateForWebState(test_web_state_.get(),
                                               suggestion_providers_);

    web_state_list_->InsertWebState(
        std::move(test_web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterIntegerPref(
        prefs::kIosPasswordBottomSheetDismissCount, 0);

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    mediator_ = [[PasswordSuggestionBottomSheetMediator alloc]
          initWithWebStateList:web_state_list_.get()
                 faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                   profile_.get())
                   prefService:prefs_.get()
                        params:params_
                  reauthModule:nil
                           URL:URL()
          profilePasswordStore:store_
          accountPasswordStore:nullptr
        sharedURLLoaderFactory:nullptr
             engagementTracker:nil];
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
        @[ [PasswordSuggestionBottomSheetMediatorTestSuggestionProvider
            providerWithSuggestions] ]);
  }

  GURL URL() { return GURL("http://foo.com"); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<password_manager::TestPasswordStore> store_;
  id consumer_;
  NSArray<id<FormSuggestionProvider>>* suggestion_providers_;
  autofill::FormActivityParams params_;
  PasswordSuggestionBottomSheetMediator* mediator_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

// Tests PasswordSuggestionBottomSheetMediator can be initialized.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, Init) {
  CreateMediator();
  EXPECT_TRUE(mediator_);
}

// Tests consumer when no suggestion is available.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, NoSuggestion) {
  CreateMediator();
  ASSERT_TRUE(mediator_);

  OCMExpect([consumer_ dismiss]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, WithSuggestions) {
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
TEST_F(PasswordSuggestionBottomSheetMediatorTest,
       WithSuggestions_ForSingleUsernameForm_FeatureEnabled) {
  id<FormSuggestionProvider> provider =
      [[PasswordSuggestionBottomSheetMediatorTestSuggestionProvider alloc]
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

TEST_F(PasswordSuggestionBottomSheetMediatorTest, IncrementDismissCount) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);

  EXPECT_EQ(
      prefs_.get()->GetInteger(prefs::kIosPasswordBottomSheetDismissCount), 0);
  [mediator_ dismiss];
  EXPECT_EQ(
      prefs_.get()->GetInteger(prefs::kIosPasswordBottomSheetDismissCount), 1);
  [mediator_ dismiss];
  EXPECT_EQ(
      prefs_.get()->GetInteger(prefs::kIosPasswordBottomSheetDismissCount), 2);
  [mediator_ dismiss];
  EXPECT_EQ(
      prefs_.get()->GetInteger(prefs::kIosPasswordBottomSheetDismissCount), 3);

  // Expect failure after 3 times.
#if defined(GTEST_HAS_DEATH_TEST)
  EXPECT_DEATH([mediator_ dismiss],
               "Failed when dismiss count is incremented higher than the "
               "expected value.");
#endif  // defined(GTEST_HAS_DEATH_TEST)
}

TEST_F(PasswordSuggestionBottomSheetMediatorTest, SuggestionUsernameHasSuffix) {
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
      suggestionWithValue:[NSString
                              stringWithFormat:@"%@%@", @"test1",
                                               kPasswordFormSuggestionSuffix]
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  std::optional<password_manager::CredentialUIEntry> credential =
      [mediator_ getCredentialForFormSuggestion:suggestion];
  EXPECT_TRUE(credential.has_value());
  EXPECT_EQ(credential.value(), expectedCredential);
}

TEST_F(PasswordSuggestionBottomSheetMediatorTest,
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
        backendIdentifier:nil
           requiresReauth:NO];
  std::optional<password_manager::CredentialUIEntry> credential =
      [mediator_ getCredentialForFormSuggestion:suggestion];
  EXPECT_TRUE(credential.has_value());
  EXPECT_EQ(credential.value(), expectedCredential);
}

// Tests that the mediator is correctly cleaned up when the WebStateList is
// destroyed. There are a lot of checked observer lists that could potentially
// cause a crash in the process, so this test ensures they're executed.
TEST_F(PasswordSuggestionBottomSheetMediatorTest,
       CleansUpWhenWebStateListDestroyed) {
  CreateMediatorWithDefaultSuggestions();
  ASSERT_TRUE(mediator_);
  [mediator_ setConsumer:consumer_];

  OCMExpect([consumer_ dismiss]);
  web_state_list_.reset();
  EXPECT_OCMOCK_VERIFY(consumer_);
}
