// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"

#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/filling_product.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/plus_addresses/features.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::FieldRendererId;
using autofill::FillingProduct;
using autofill::FormRendererId;

// Test provider that records invocations of its interface methods.
@interface TestSuggestionProvider : NSObject<FormSuggestionProvider>

@property(weak, nonatomic, readonly) FormSuggestion* suggestion;
@property(nonatomic, assign) NSInteger index;
@property(weak, nonatomic, readonly) NSString* formName;
@property(weak, nonatomic, readonly) NSString* fieldIdentifier;
@property(weak, nonatomic, readonly) NSString* frameID;
@property(nonatomic, assign) BOOL selected;
@property(nonatomic, assign) BOOL askedIfSuggestionsAvailable;
@property(nonatomic, assign) BOOL askedForSuggestions;
@property(nonatomic, assign) SuggestionProviderType type;
@property(nonatomic, readonly) FillingProduct mainFillingProduct;

// Creates a test provider with default suggesstions.
+ (instancetype)providerWithSuggestions;

- (instancetype)initWithSuggestions:(NSArray*)suggestions;

@end

@implementation TestSuggestionProvider {
  NSArray* _suggestions;
  NSString* _formName;
  FormRendererId _formRendererID;
  NSString* _fieldIdentifier;
  FieldRendererId _fieldRendererID;
  NSString* _frameID;
  FormSuggestion* _suggestion;
}

@synthesize selected = _selected;
@synthesize askedIfSuggestionsAvailable = _askedIfSuggestionsAvailable;
@synthesize askedForSuggestions = _askedForSuggestions;

+ (instancetype)providerWithSuggestions {
  NSArray* suggestions = @[
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
  return [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
}

- (instancetype)initWithSuggestions:(NSArray*)suggestions {
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
             formRendererID:(FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(FieldRendererId)fieldRendererID
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

namespace {

// Test fixture for FormSuggestionController testing.
class FormSuggestionControllerTest : public PlatformTest {
 public:
  FormSuggestionControllerTest()
      : test_form_activity_tab_helper_(&fake_web_state_) {}

  FormSuggestionControllerTest(const FormSuggestionControllerTest&) = delete;
  FormSuggestionControllerTest& operator=(const FormSuggestionControllerTest&) =
      delete;

  void SetUp() override {
    PlatformTest::SetUp();

    fake_web_state_.SetWebViewProxy(mock_web_view_proxy_);
  }

  void TearDown() override {
    [accessory_mediator_ disconnect];
    [suggestion_controller_ detachFromWebState];
    PlatformTest::TearDown();
  }

 protected:
  // Sets up `suggestion_controller_` with the specified array of
  // FormSuggestionProviders.
  void SetUpController(NSArray* providers) {
    suggestion_controller_ =
        [[FormSuggestionController alloc] initWithWebState:&fake_web_state_
                                                 providers:providers];
    [suggestion_controller_ setWebViewProxy:mock_web_view_proxy_];

    id mock_consumer = [OCMockObject
        niceMockForProtocol:@protocol(FormInputAccessoryConsumer)];
    // Mock the consumer to verify the suggestion views.
    void (^mockShow)(NSInvocation*) = ^(NSInvocation* invocation) {
      __unsafe_unretained NSArray* suggestions;
      [invocation getArgument:&suggestions atIndex:2];
      received_suggestions_ = suggestions;
    };
    [[[mock_consumer stub] andDo:mockShow]
        showAccessorySuggestions:[OCMArg any]];

    id mock_window = OCMClassMock([UIWindow class]);

    id mock_web_state_view = OCMClassMock([UIView class]);
    OCMStub([mock_web_state_view window]).andReturn(mock_window);

    fake_web_state_.SetView(mock_web_state_view);

    mock_handler_ =
        OCMProtocolMock(@protocol(FormInputAccessoryMediatorHandler));

    accessory_mediator_ =
        [[FormInputAccessoryMediator alloc] initWithConsumer:mock_consumer
                                                     handler:mock_handler_
                                                webStateList:nullptr
                                         personalDataManager:nullptr
                                        profilePasswordStore:nullptr
                                        accountPasswordStore:nullptr
                                        securityAlertHandler:nil
                                      reauthenticationModule:nil
                                           engagementTracker:nil];

    [accessory_mediator_ injectWebState:&fake_web_state_];
    [accessory_mediator_ injectProvider:suggestion_controller_];
  }

  // The scoped feature list to enable/disable features. This needs to be placed
  // before task_environment_, as per
  // https://source.chromium.org/chromium/chromium/src/+/main:base/test/scoped_feature_list.h;l=37-41;drc=fe05104cfedb627fa99f218d7d1af6862871566c.
  base::test::ScopedFeatureList scoped_feature_list_;

  // The associated test Web Threads.
  web::WebTaskEnvironment task_environment_;

  // Installs the local state in ApplicationContext.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  // The FormSuggestionController under test.
  FormSuggestionController* suggestion_controller_;

  // The suggestions the controller sent to the client, if any.
  NSArray* received_suggestions_;

  // Mock CRWWebViewProxy for verifying interactions.
  id mock_web_view_proxy_;

  // Accessory view controller.
  FormInputAccessoryMediator* accessory_mediator_;

  // The fake WebState to simulate navigation and JavaScript events.
  web::FakeWebState fake_web_state_;

  // The fake form tracker to simulate form events.
  autofill::TestFormActivityTabHelper test_form_activity_tab_helper_;

  // Mock FormInputAccessoryMediatorHandler for verifying interactions.
  id mock_handler_;
};

// Tests that pages whose URLs don't have a web scheme aren't processed.
TEST_F(FormSuggestionControllerTest, PageLoadShouldBeIgnoredWhenNotWebScheme) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  fake_web_state_.SetCurrentURL(GURL("data:text/html;charset=utf8;base64,"));
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that pages whose content isn't HTML aren't processed.
TEST_F(FormSuggestionControllerTest, PageLoadShouldBeIgnoredWhenNotHtml) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  // Load PDF file URL.
  fake_web_state_.SetContentIsHTML(false);
  fake_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that the suggestions are reset when a navigation is finished.
TEST_F(FormSuggestionControllerTest,
       PageLoadShouldRestoreKeyboardAccessoryViewAndInjectJavaScript) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  // Trigger form activity, which should set up the suggestions view.
  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);
  EXPECT_TRUE(received_suggestions_.count);

  // Trigger another navigation. The suggestions should not be present.
  web::FakeNavigationContext navigation_context;
  fake_web_state_.OnNavigationFinished(&navigation_context);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that "blur" events are ignored.
TEST_F(FormSuggestionControllerTest, FormActivityBlurShouldBeIgnored) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "blur";  // blur!
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that no suggestions are displayed when no providers are registered.
TEST_F(FormSuggestionControllerTest,
       FormActivityShouldRetrieveSuggestions_NoProvidersAvailable) {
  // Set up the controller without any providers.
  SetUpController(@[]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);

  // The suggestions should be empty.
  EXPECT_TRUE(received_suggestions_);
  EXPECT_EQ(0U, received_suggestions_.count);
}

// Tests that, when no providers have suggestions to offer for a form/field,
// they aren't asked and no suggestions are displayed.
TEST_F(FormSuggestionControllerTest,
       FormActivityShouldRetrieveSuggestions_NoSuggestionsAvailable) {
  // Set up the controller with some providers, but none of them will
  // have suggestions available.
  TestSuggestionProvider* provider1 =
      [[TestSuggestionProvider alloc] initWithSuggestions:@[]];
  TestSuggestionProvider* provider2 =
      [[TestSuggestionProvider alloc] initWithSuggestions:@[]];
  SetUpController(@[ provider1, provider2 ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);

  // The providers should each be asked if they have suggestions for the
  // form in question.
  EXPECT_TRUE([provider1 askedIfSuggestionsAvailable]);
  EXPECT_TRUE([provider2 askedIfSuggestionsAvailable]);

  // Since none of the providers had suggestions available, none of them
  // should have been asked for suggestions.
  EXPECT_FALSE([provider1 askedForSuggestions]);
  EXPECT_FALSE([provider2 askedForSuggestions]);

  // The suggestions should be empty.
  EXPECT_FALSE(received_suggestions_.count);
  EXPECT_EQ(0U, received_suggestions_.count);
}

// Tests that, once a provider is asked if it has suggestions for a form/field,
// it and only it is asked to provide them, and that suggestions are then sent.
TEST_F(FormSuggestionControllerTest,
       FormActivityShouldRetrieveSuggestions_SuggestionsAddedToAccessoryView) {
  // Set up the controller with some providers, one of which can provide
  // suggestions.
  NSArray* suggestions = @[
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
  TestSuggestionProvider* provider1 =
      [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
  TestSuggestionProvider* provider2 =
      [[TestSuggestionProvider alloc] initWithSuggestions:@[]];
  SetUpController(@[ provider1, provider2 ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);

  // Since the first provider has suggestions available, it and only it
  // should have been asked.
  EXPECT_TRUE([provider1 askedIfSuggestionsAvailable]);
  EXPECT_FALSE([provider2 askedIfSuggestionsAvailable]);

  // Since the first provider said it had suggestions, it and only it
  // should have been asked to provide them.
  EXPECT_TRUE([provider1 askedForSuggestions]);
  EXPECT_FALSE([provider2 askedForSuggestions]);

  // The controller should have provided suggestions.
  EXPECT_TRUE(received_suggestions_.count);
  EXPECT_NSEQ(suggestions, received_suggestions_);
}

// Tests that selecting a suggestion informs the specified delegate for that
// suggestion.
TEST_F(FormSuggestionControllerTest, SelectingSuggestionShouldNotifyDelegate) {
  // Send some suggestions to the controller and then tap one.
  NSArray* suggestions = @[
    [FormSuggestion
        suggestionWithValue:@"foo"
         displayDescription:nil
                       icon:nil
                       type:autofill::SuggestionType::kAutocompleteEntry
          backendIdentifier:nil
             requiresReauth:NO],
  ];
  TestSuggestionProvider* provider =
      [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
  SetUpController(@[ provider ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.frame_id = "frame_id";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);

  // Selecting a suggestion should notify the delegate.
  [suggestion_controller_ didSelectSuggestion:suggestions[0] atIndex:0];
  EXPECT_TRUE([provider selected]);
  EXPECT_NSEQ(@"form", [provider formName]);
  EXPECT_NSEQ(@"field_id", [provider fieldIdentifier]);
  EXPECT_NSEQ(@"frame_id", [provider frameID]);
  EXPECT_NSEQ(suggestions[0], [provider suggestion]);
}

// Tests that the autofill suggestion IPH is triggered when suggesting an
// address if the suggestion's `featureForiPH` property is set.
TEST_F(FormSuggestionControllerTest, AutofillSuggestionIPH) {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@"foo"
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kAutocompleteEntry
        backendIdentifier:nil
           requiresReauth:NO];
  suggestion.featureForIPH =
      SuggestionFeatureForIPH::kAutofillExternalAccountProfile;
  NSArray* suggestions = @[ suggestion ];
  TestSuggestionProvider* provider =
      [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
  provider.type = SuggestionProviderTypeAutofill;
  SetUpController(@[ provider ]);
  GURL url("http://foo.com");
  fake_web_state_.SetCurrentURL(url);
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
  autofill::FormActivityParams params;

  OCMExpect([mock_handler_
      showAutofillSuggestionIPHIfNeededFor:suggestion.featureForIPH]);
  test_form_activity_tab_helper_.FormActivityRegistered(main_frame.get(),
                                                        params);
  [mock_handler_ verify];
}

// Tests that password generation suggestions always have an icon.
TEST_F(FormSuggestionControllerTest, CopyAndAdjustSuggestions) {
  base::test::ScopedFeatureList feature_list(kIOSKeyboardAccessoryUpgrade);
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }

  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@""
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kGeneratePasswordEntry
        backendIdentifier:nil
           requiresReauth:NO];
  [suggestions addObject:suggestion];

  NSArray<FormSuggestion*>* adjusted_suggestions =
      [suggestion_controller_ copyAndAdjustSuggestions:suggestions];
  EXPECT_TRUE(adjusted_suggestions.count);
  EXPECT_TRUE(adjusted_suggestions[0].icon);
}

// Tests that plus address suggestions always have an icon when the features are
// enabled.
TEST_F(FormSuggestionControllerTest, CopyAndAdjustPlusAddressSuggestions) {
  base::test::ScopedFeatureList feature_list{
      plus_addresses::features::kPlusAddressesEnabled};

  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);

  NSMutableArray<FormSuggestion*>* suggestions = [NSMutableArray array];
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:@""
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kCreateNewPlusAddress
        backendIdentifier:nil
           requiresReauth:NO];
  [suggestions addObject:suggestion];

  suggestion = [FormSuggestion
      suggestionWithValue:@""
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kFillExistingPlusAddress
        backendIdentifier:nil
           requiresReauth:NO];
  [suggestions addObject:suggestion];

  NSArray<FormSuggestion*>* adjusted_suggestions =
      [suggestion_controller_ copyAndAdjustSuggestions:suggestions];
  EXPECT_EQ(adjusted_suggestions.count, suggestions.count);
  EXPECT_TRUE(adjusted_suggestions[0].icon);
  EXPECT_TRUE(adjusted_suggestions[1].icon);
}

}  // namespace
