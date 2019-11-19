// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_controller.h"

#include <utility>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_consumer.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test provider that records invocations of its interface methods.
@interface TestSuggestionProvider : NSObject<FormSuggestionProvider>

@property(weak, nonatomic, readonly) FormSuggestion* suggestion;
@property(weak, nonatomic, readonly) NSString* formName;
@property(weak, nonatomic, readonly) NSString* fieldIdentifier;
@property(weak, nonatomic, readonly) NSString* frameID;
@property(nonatomic, assign) BOOL selected;
@property(nonatomic, assign) BOOL askedIfSuggestionsAvailable;
@property(nonatomic, assign) BOOL askedForSuggestions;

// Creates a test provider with default suggesstions.
+ (instancetype)providerWithSuggestions;

- (instancetype)initWithSuggestions:(NSArray*)suggestions;

@end

@implementation TestSuggestionProvider {
  NSArray* _suggestions;
  NSString* _formName;
  NSString* _fieldIdentifier;
  NSString* _frameID;
  FormSuggestion* _suggestion;
}

@synthesize selected = _selected;
@synthesize askedIfSuggestionsAvailable = _askedIfSuggestionsAvailable;
@synthesize askedForSuggestions = _askedForSuggestions;

+ (instancetype)providerWithSuggestions {
  NSArray* suggestions = @[
    [FormSuggestion suggestionWithValue:@"foo"
                     displayDescription:nil
                                   icon:@""
                             identifier:0],
    [FormSuggestion suggestionWithValue:@"bar"
                     displayDescription:nil
                                   icon:@""
                             identifier:1]
  ];
  return [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
}

- (instancetype)initWithSuggestions:(NSArray*)suggestions {
  self = [super init];
  if (self)
    _suggestions = [suggestions copy];
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

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  self.askedIfSuggestionsAvailable = YES;
  completion([_suggestions count] > 0);
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  self.askedForSuggestions = YES;
  completion(_suggestions, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  self.selected = YES;
  _suggestion = suggestion;
  _formName = [formName copy];
  _fieldIdentifier = [fieldIdentifier copy];
  _frameID = [frameID copy];
  completion();
}

@end

namespace {

// Test fixture for FormSuggestionController testing.
class FormSuggestionControllerTest : public PlatformTest {
 public:
  FormSuggestionControllerTest()
      : test_form_activity_tab_helper_(&test_web_state_) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Mock out the JsSuggestionManager.
    mock_js_suggestion_manager_ =
        [OCMockObject niceMockForClass:[JsSuggestionManager class]];

    test_web_state_.SetWebViewProxy(mock_web_view_proxy_);
  }

  void TearDown() override {
    [suggestion_controller_ detachFromWebState];
    PlatformTest::TearDown();
  }

 protected:
  // Sets up |suggestion_controller_| with the specified array of
  // FormSuggestionProviders.
  void SetUpController(NSArray* providers) {
    suggestion_controller_ = [[FormSuggestionController alloc]
           initWithWebState:&test_web_state_
                  providers:providers
        JsSuggestionManager:mock_js_suggestion_manager_];
    [suggestion_controller_ setWebViewProxy:mock_web_view_proxy_];

    id mock_consumer_ = [OCMockObject
        niceMockForProtocol:@protocol(FormInputAccessoryConsumer)];
    // Mock the consumer to verify the suggestion views.
    void (^mockShow)(NSInvocation*) = ^(NSInvocation* invocation) {
      __unsafe_unretained NSArray* suggestions;
      [invocation getArgument:&suggestions atIndex:2];
      received_suggestions_ = suggestions;
    };
    [[[mock_consumer_ stub] andDo:mockShow]
        showAccessorySuggestions:[OCMArg any]
                suggestionClient:[OCMArg any]];

    // Mock restore keyboard to verify cleanup.
    void (^mockRestore)(NSInvocation*) = ^(NSInvocation* invocation) {
      received_suggestions_ = nil;
    };
    [[[mock_consumer_ stub] andDo:mockRestore] restoreOriginalKeyboardView];

    accessory_mediator_ =
        [[FormInputAccessoryMediator alloc] initWithConsumer:mock_consumer_
                                                    delegate:nil
                                                webStateList:NULL
                                         personalDataManager:NULL
                                               passwordStore:nullptr];

    [accessory_mediator_ injectWebState:&test_web_state_];
    [accessory_mediator_ injectProvider:suggestion_controller_];
    [accessory_mediator_ injectSuggestionManager:mock_js_suggestion_manager_];
  }

  // The FormSuggestionController under test.
  FormSuggestionController* suggestion_controller_;

  // The suggestions the controller sent to the client, if any.
  NSArray* received_suggestions_;

  // Mock JsSuggestionManager for verifying interactions.
  id mock_js_suggestion_manager_;

  // Mock CRWWebViewProxy for verifying interactions.
  id mock_web_view_proxy_;

  // Accessory view controller.
  FormInputAccessoryMediator* accessory_mediator_;

  // The associated test Web Threads.
  web::WebTaskEnvironment task_environment_;

  // The fake WebState to simulate navigation and JavaScript events.
  web::TestWebState test_web_state_;

  // The fake form tracker to simulate form events.
  autofill::TestFormActivityTabHelper test_form_activity_tab_helper_;

  DISALLOW_COPY_AND_ASSIGN(FormSuggestionControllerTest);
};

// Tests that pages whose URLs don't have a web scheme aren't processed.
TEST_F(FormSuggestionControllerTest, PageLoadShouldBeIgnoredWhenNotWebScheme) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  test_web_state_.SetCurrentURL(GURL("data:text/html;charset=utf8;base64,"));
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(received_suggestions_.count);
  EXPECT_OCMOCK_VERIFY(mock_js_suggestion_manager_);
}

// Tests that pages whose content isn't HTML aren't processed.
TEST_F(FormSuggestionControllerTest, PageLoadShouldBeIgnoredWhenNotHtml) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  // Load PDF file URL.
  test_web_state_.SetContentIsHTML(false);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that the suggestions are reset and JavaScript is injected when a page
// is loaded.
TEST_F(FormSuggestionControllerTest,
       PageLoadShouldRestoreKeyboardAccessoryViewAndInjectJavaScript) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  GURL url("http://foo.com");
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", /*is_main_frame=*/true, url);

  // Trigger form activity, which should set up the suggestions view.
  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);
  EXPECT_TRUE(received_suggestions_.count);

  // Trigger another page load. The suggestions should not be present.
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that "blur" events are ignored.
TEST_F(FormSuggestionControllerTest, FormActivityBlurShouldBeIgnored) {
  SetUpController(@[ [TestSuggestionProvider providerWithSuggestions] ]);
  GURL url("http://foo.com");
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", true, url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "blur";  // blur!
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);
  EXPECT_FALSE(received_suggestions_.count);
}

// Tests that no suggestions are displayed when no providers are registered.
TEST_F(FormSuggestionControllerTest,
       FormActivityShouldRetrieveSuggestions_NoProvidersAvailable) {
  // Set up the controller without any providers.
  SetUpController(@[]);
  GURL url("http://foo.com");
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", true, url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);

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
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", true, url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);

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
    [FormSuggestion suggestionWithValue:@"foo"
                     displayDescription:nil
                                   icon:@""
                             identifier:0],
    [FormSuggestion suggestionWithValue:@"bar"
                     displayDescription:nil
                                   icon:@""
                             identifier:1]
  ];
  TestSuggestionProvider* provider1 =
      [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
  TestSuggestionProvider* provider2 =
      [[TestSuggestionProvider alloc] initWithSuggestions:@[]];
  SetUpController(@[ provider1, provider2 ]);
  GURL url("http://foo.com");
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", true, url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);

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
    [FormSuggestion suggestionWithValue:@"foo"
                     displayDescription:nil
                                   icon:@""
                             identifier:0],
  ];
  TestSuggestionProvider* provider =
      [[TestSuggestionProvider alloc] initWithSuggestions:suggestions];
  SetUpController(@[ provider ]);
  GURL url("http://foo.com");
  test_web_state_.SetCurrentURL(url);
  web::FakeWebFrame main_frame("main_frame", true, url);

  autofill::FormActivityParams params;
  params.form_name = "form";
  params.field_identifier = "field_id";
  params.field_type = "text";
  params.type = "type";
  params.value = "value";
  params.frame_id = "frame_id";
  params.input_missing = false;
  test_form_activity_tab_helper_.FormActivityRegistered(&main_frame, params);

  // Selecting a suggestion should notify the delegate.
  [suggestion_controller_ didSelectSuggestion:suggestions[0]];
  EXPECT_TRUE([provider selected]);
  EXPECT_NSEQ(@"form", [provider formName]);
  EXPECT_NSEQ(@"field_id", [provider fieldIdentifier]);
  EXPECT_NSEQ(@"frame_id", [provider frameID]);
  EXPECT_NSEQ(suggestions[0], [provider suggestion]);
}

}  // namespace
