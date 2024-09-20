// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <memory>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/types/id_type.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/address_data_manager_test_api.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager_test_utils.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#import "components/autofill/core/common/autofill_clock.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ScopedFeatureList;

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface TestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;
@property(nonatomic, assign) BOOL suggestionRetrievalComplete;
@property(nonatomic, assign) BOOL suggestionRetrievalStarted;

@end

@implementation TestSuggestionController

@synthesize suggestions = _suggestions;
@synthesize suggestionRetrievalComplete = _suggestionRetrievalComplete;

- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState {
  self.suggestionRetrievalStarted = YES;
  [super retrieveSuggestionsForForm:params webState:webState];
}

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
  self.suggestionRetrievalComplete = YES;
}

- (void)onNoSuggestionsAvailable {
  self.suggestionRetrievalComplete = YES;
}

- (void)resetSuggestionAvailable {
  self.suggestionRetrievalComplete = NO;
  self.suggestionRetrievalStarted = NO;
  self.suggestions = nil;
}

@end

namespace autofill {

namespace {

// The profile-type form used by tests.
NSString* const kProfileFormHtml =
    @"<form action='/submit' method='post'>"
     "Name <input type='text' name='name'>"
     "Address <input type='text' name='address'>"
     "City <input type='text' name='city'>"
     "State <input type='text' name='state'>"
     "Zip <input type='text' name='zip'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// A minimal form with a name.
NSString* const kMinimalFormWithNameHtml = @"<form id='form1'>"
                                            "<input name='name'>"
                                            "<input name='address'>"
                                            "<input name='city'>"
                                            "</form>";

// The key/value-type form used by tests.
NSString* const kKeyValueFormHtml =
    @"<form action='/submit' method='post'>"
     "Greeting <input id='greeting' type='text' name='greeting'>"
     "Dummy field <input id='dummy' type='text' name='dummy'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// The credit card-type form used by tests.
NSString* const kCreditCardFormHtml =
    @"<form id='form' action='/submit' method='post'>"
     "Name on card: <input id='name' type='text' name='name'>"
     "Credit card number: <input id='CCNo' type='text' name='CCNo'>"
     "Expiry Month: <input id='CCExpiresMonth' type='text' "
     "name='CCExpiresMonth'>"
     "Expiry Year: <input id='CCExpiresYear' type='text' name='CCExpiresYear'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// An HTML page without a card-type form.
static NSString* kNoCreditCardFormHtml =
    @"<form><input type=\"text\" autofocus autocomplete=\"username\"></form>";

// A credit card-type form with the autofocus attribute (which is detected at
// page load).
NSString* const kCreditCardAutofocusFormHtml =
    @"<form><input type=\"text\" autofocus autocomplete=\"cc-number\"></form>";

// A profile-type formless form. The fields are not inside a form element.
NSString* const kProfileFormlessHtml =
    @"<div id='div'>"
     "Name <input id='name' type='text' name='name'>"
     "Address <input id='address' type='text' name='address'>"
     "City <input id='city' type='text' name='city'>"
     "State <input id='state' type='text' name='state'>"
     "Zip <input id='zip' type='text' name='zip'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</div>";

using ::testing::AllOf;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::IsTrue;
using ::testing::Property;

// FAIL if a field with the supplied `name` and `fieldType` is not present on
// the `form`.
void CheckField(const FormStructure& form,
                FieldType fieldType,
                const char* name) {
  for (const auto& field : form) {
    if (field->heuristic_type() == fieldType) {
      EXPECT_EQ(base::UTF8ToUTF16(name), field->name());
      return;
    }
  }
  FAIL() << "Missing field " << name;
}

AutocompleteEntry CreateAutocompleteEntry(const std::u16string& value) {
  const base::Time kNow = AutofillClock::Now();
  return AutocompleteEntry(AutocompleteKey(u"Name", value), kNow, kNow);
}

// Forces rendering of a UIView. This is used in tests to make sure that UIKit
// optimizations don't have the views return the previous values (such as
// zoomScale).
void ForceViewRendering(UIView* view) {
  EXPECT_TRUE(view);
  CALayer* layer = view.layer;
  EXPECT_TRUE(layer);
  const CGFloat kArbitraryNonZeroPositiveValue = 19;
  const CGSize arbitraryNonEmptyArea = CGSizeMake(
      kArbitraryNonZeroPositiveValue, kArbitraryNonZeroPositiveValue);
  UIGraphicsBeginImageContext(arbitraryNonEmptyArea);
  CGContext* context = UIGraphicsGetCurrentContext();
  EXPECT_TRUE(context);
  [layer renderInContext:context];
  UIGraphicsEndImageContext();
}

// Returns a matcher to verify a child frame in the FormData.
auto ChildFrameMatcher(int expected_predecessor) {
  const auto valid_token_matcher = ::testing::Field(
      &FrameTokenWithPredecessor::token,
      ::testing::VariantWith<RemoteFrameToken>(::testing::IsTrue()));
  const auto predecessor_matcher =
      ::testing::Field(&FrameTokenWithPredecessor::predecessor,
                       testing::Eq(expected_predecessor));
  return AllOf(valid_token_matcher, predecessor_matcher);
}

// WebDataServiceConsumer for receiving vectors of strings and making them
// available to tests.
class TestConsumer : public WebDataServiceConsumer {
 public:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK_EQ(result->GetType(), AUTOFILL_VALUE_RESULT);
    result_ =
        static_cast<WDResult<std::vector<AutocompleteEntry>>*>(result.get())
            ->GetValue();
  }
  std::vector<AutocompleteEntry> result_;
};

// Text fixture to test autofill.
class AutofillControllerTest : public PlatformTest {
 public:
  AutofillControllerTest() : web_client_(std::make_unique<ChromeWebClient>()) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    // Profile import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestProfileIOS by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  AutofillControllerTest(const AutofillControllerTest&) = delete;
  AutofillControllerTest& operator=(const AutofillControllerTest&) = delete;

  ~AutofillControllerTest() override {}

 protected:
  class TestAutofillClient : public ChromeAutofillClientIOS {
   public:
    using ChromeAutofillClientIOS::ChromeAutofillClientIOS;
    AutofillCrowdsourcingManager* GetCrowdsourcingManager() override {
      return nullptr;
    }
  };

  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(AutofillDriverIOS* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    TestAutofillManagerWaiter& waiter() { return waiter_; }

   private:
    TestAutofillManagerWaiter waiter_{*this,
                                      {AutofillManagerEvent::kFormsSeen}};
  };

  void SetUp() override;
  void TearDown() override;

  void SetUpForSuggestions(NSString* data, size_t expected_number_of_forms);

  // Adds key value data to the Personal Data Manager and loads test page.
  void SetUpKeyValueData();

  // Blocks until suggestion retrieval has completed.
  // If `wait_for_trigger` is yes, wait for the call to
  // `retrieveSuggestionsForForm` to avoid considering a former call.
  void WaitForSuggestionRetrieval(BOOL wait_for_trigger);
  void ResetWaitForSuggestionRetrieval();

  // Loads the page and wait until the initial form processing has been done.
  // This processing must find `expected_size` forms.
  [[nodiscard]] bool LoadHtmlAndWaitForFormFetched(
      NSString* html,
      size_t expected_number_of_forms,
      size_t expected_number_of_calls = 1);

  void LoadHtmlAndInitRendererIds(NSString* html);

  // Fails if the specified metric was not registered the given number of times.
  void ExpectMetric(const std::string& histogram_name, int sum);

  TestSuggestionController* suggestion_controller() {
    return suggestion_controller_;
  }

  void WaitForCondition(ConditionBlock condition);

  // Simulates a text input event by focusing the field with 'field_id' and
  // dispatching a TextEvent with value 'field_value'.
  void SimulateTextInputEvent(NSString* field_id, NSString* field_value);

  // Returns the AutofillManager for the main frame.
  BrowserAutofillManager* autofill_manager_for_main_frame() {
    web::WebFramesManager* frames_manager =
        AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state());
    web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
    return &AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
                ->GetAutofillManager();
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  bool processed_a_task_ = false;
  // Histogram tester for these tests.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  std::unique_ptr<autofill::AutofillClient> autofill_client_;

  AutofillAgent* autofill_agent_;

  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;

  // Retrieves suggestions according to form events.
  TestSuggestionController* suggestion_controller_;

  // Retrieves accessory views according to form events.
  FormInputAccessoryMediator* accessory_mediator_;

  PasswordController* passwordController_;
};

void AutofillControllerTest::SetUp() {
  PlatformTest::SetUp();

  // Create a PasswordController instance that will handle set up for renderer
  // ids.
  passwordController_ =
      [[PasswordController alloc] initWithWebState:web_state()];

  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:profile_->GetPrefs()
                                        webState:web_state()];
  suggestion_controller_ =
      [[TestSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofill_agent_ ]];

  InfoBarManagerImpl::CreateForWebState(web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  autofill_client_ = std::make_unique<TestAutofillClient>(
      profile_.get(), web_state(), infobar_manager, autofill_agent_);

  autofill_client_->GetPersonalDataManager()
      ->address_data_manager()
      .get_alternative_state_name_map_updater_for_testing()
      ->set_local_state_for_testing(local_state());

  std::string locale("en");
  autofill::AutofillDriverIOSFactory::CreateForWebState(
      web_state(), autofill_client_.get(), /*autofill_agent=*/nil, locale);

  autofill_manager_injector_ =
      std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
          web_state());

  accessory_mediator_ =
      [[FormInputAccessoryMediator alloc] initWithConsumer:nil
                                                   handler:nil
                                              webStateList:nullptr
                                       personalDataManager:nullptr
                                      profilePasswordStore:nullptr
                                      accountPasswordStore:nullptr
                                      securityAlertHandler:nil
                                    reauthenticationModule:nil
                                         engagementTracker:nil];

  [accessory_mediator_ injectWebState:web_state()];
  [accessory_mediator_ injectProvider:suggestion_controller_];

  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void AutofillControllerTest::TearDown() {
  [accessory_mediator_ disconnect];
  [suggestion_controller_ detachFromWebState];

  web::test::WaitForBackgroundTasks();
  web_state_.reset();
}

void AutofillControllerTest::ResetWaitForSuggestionRetrieval() {
  [suggestion_controller() resetSuggestionAvailable];
}

void AutofillControllerTest::WaitForSuggestionRetrieval(BOOL wait_for_trigger) {
  // Wait for the message queue to ensure that JS events fired in the tests
  // trigger TestSuggestionController's retrieveSuggestionsForFormNamed: method
  // and set suggestionRetrievalComplete to NO.
  if (wait_for_trigger) {
    WaitForCondition(^bool {
      return suggestion_controller().suggestionRetrievalStarted;
    });
  }
  // Now we can wait for suggestionRetrievalComplete to be set to YES.
  WaitForCondition(^bool {
    return suggestion_controller().suggestionRetrievalComplete;
  });
}

bool AutofillControllerTest::LoadHtmlAndWaitForFormFetched(
    NSString* html,
    size_t expected_number_of_forms,
    size_t expected_number_of_calls) {
  web::test::LoadHtml(html, web_state());
  TestAutofillManager* autofill_manager =
      autofill_manager_injector_->GetForMainFrame();
  return autofill_manager->waiter().Wait(expected_number_of_calls) &&
         autofill_manager->form_structures().size() == expected_number_of_forms;
}

void AutofillControllerTest::ExpectMetric(const std::string& histogram_name,
                                          int sum) {
  histogram_tester_->ExpectBucketCount(histogram_name, sum, 1);
}

void AutofillControllerTest::WaitForCondition(ConditionBlock condition) {
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1000),
                                                           true, condition));
}

void AutofillControllerTest::SimulateTextInputEvent(NSString* field_id,
                                                    NSString* field_value) {
  // First focus the field, otherwise the input event does not get delivered to
  // the browser process.
  // Then create and dispatch a TextEvent from the field with the given id.
  web::test::ExecuteJavaScript(
      [NSString
          stringWithFormat:
              @"document.getElementById('%@').focus();"
              @"var event = document.createEvent('TextEvent');"
              @"event.initTextEvent('textInput', true, true, window, '%@');"
              @"document.getElementById('%@').dispatchEvent(event);",
              field_id, field_value, field_id],
      web_state());
}

// Checks that viewing an HTML page containing a form results in the form being
// registered as a FormStructure by the BrowserAutofillManager.
TEST_F(AutofillControllerTest, ReadForm) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
  BrowserAutofillManager& autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
          ->GetAutofillManager();
  const auto& forms = autofill_manager.form_structures();
  const auto& form = *(forms.begin()->second);
  CheckField(form, NAME_FULL, "name");
  CheckField(form, ADDRESS_HOME_LINE1, "address");
  CheckField(form, ADDRESS_HOME_CITY, "city");
  CheckField(form, ADDRESS_HOME_STATE, "state");
  CheckField(form, ADDRESS_HOME_ZIP, "zip");
  ExpectMetric("Autofill.IsEnabled.PageLoad", 1);
}

// Checks that when autofill across iframes is enabled the child frames are
// carried over for their parent form.
TEST_F(AutofillControllerTest, ReadForm_WithChildFrames) {
  ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutofillAcrossIframesIos);

  // A form with iframes and inputs where some of the iframes have predecessors.
  NSString* const test_page =
      @"<form id='form1'>"
       "<iframe></iframe>"
       "Name <input id='name' type='text' name='name' />"
       "<iframe></iframe>"
       "<iframe></iframe>"
       "Address <input type='text' name='address'>"
       "City <input type='text' name='city'>"
       "<iframe></iframe>"
       "State <input type='text' name='state'>"
       "</form>";

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(test_page,
                                            /*expected_number_of_forms=*/1,
                                            /*expected_number_of_calls=*/5));

  // Verify that the child frames are present in the form data.
  std::vector<FormData> form_data;
  for (const auto& [_, form] :
       autofill_manager_for_main_frame()->form_structures()) {
    form_data.push_back(form->ToFormData());
  }
  EXPECT_THAT(
      form_data,
      ElementsAre(AllOf(
          Property(&FormData::renderer_id, IsTrue()),
          Property(&FormData::child_frames,
                   ElementsAre(ChildFrameMatcher(-1), ChildFrameMatcher(0),
                               ChildFrameMatcher(0), ChildFrameMatcher(2))))));
}

// Checks that when autofill across iframes is enabled the child frames are
// carried over for their synthetic form.
TEST_F(AutofillControllerTest, ReadForm_WithChildFrames_Synthetic) {
  ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAutofillAcrossIframesIos);

  // A syntethic form with iframes and inputs where some of the iframes have
  // predecessors.
  NSString* const test_page =
      @"<html><body><div id='div'>"
       "<iframe></iframe>"
       "Name <input id='name' type='text' name='name' />"
       "<iframe></iframe>"
       "<iframe></iframe>"
       "Address <input type='text' name='address'>"
       "City <input type='text' name='city'>"
       "<iframe></iframe>"
       "State <input type='text' name='state'>"
       "</div></html></body>";

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(test_page,
                                            /*expected_number_of_forms=*/1,
                                            /*expected_number_of_calls=*/3));

  // Verify that the child frames are present in the form data.
  std::vector<FormData> form_data;
  for (const auto& [_, form] :
       autofill_manager_for_main_frame()->form_structures()) {
    form_data.push_back(form->ToFormData());
  }
  EXPECT_THAT(
      form_data,
      ElementsAre(AllOf(
          Property(&FormData::renderer_id, ::testing::IsFalse()),
          Property(&FormData::child_frames,
                   ElementsAre(ChildFrameMatcher(-1), ChildFrameMatcher(0),
                               ChildFrameMatcher(0), ChildFrameMatcher(2))))));
}

// Checks that viewing an HTML page containing a form with an 'id' results in
// the form being registered as a FormStructure by the BrowserAutofillManager,
// and the name is correctly set.
TEST_F(AutofillControllerTest, ReadFormName) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kMinimalFormWithNameHtml, 1));
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
  BrowserAutofillManager& autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
          ->GetAutofillManager();
  const auto& forms = autofill_manager.form_structures();
  const auto& form = *(forms.begin()->second);
  EXPECT_EQ(u"form1", form.ToFormData().name());
}

// Checks that an HTML page containing a profile-type form which is submitted
// with scripts (simulating user form submission) results in a profile being
// successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, ProfileImport) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  test_api(personal_data_manager->address_data_manager())
      .set_auto_accept_address_imports(true);
  // Check there are no registered profiles already.
  EXPECT_EQ(0U,
            personal_data_manager->address_data_manager().GetProfiles().size());
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  web::test::ExecuteJavaScript(
      @"document.forms[0].name.value = 'Homer Simpson'", web_state());
  web::test::ExecuteJavaScript(
      @"document.forms[0].address.value = '123 Main Street'", web_state());
  web::test::ExecuteJavaScript(@"document.forms[0].city.value = 'Springfield'",
                               web_state());
  web::test::ExecuteJavaScript(@"document.forms[0].state.value = 'IL'",
                               web_state());
  web::test::ExecuteJavaScript(@"document.forms[0].zip.value = '55123'",
                               web_state());
  web::test::ExecuteJavaScript(@"submit.click()", web_state());
  WaitForCondition(^bool {
    return personal_data_manager->address_data_manager().GetProfiles().size();
  });
  const std::vector<const AutofillProfile*>& profiles =
      personal_data_manager->address_data_manager().GetProfiles();
  if (profiles.size() != 1) {
    FAIL() << "Not exactly one profile found after attempted import";
  }
  const AutofillProfile& profile = *profiles[0];
  EXPECT_EQ(u"Homer Simpson", profile.GetInfo(NAME_FULL, "en-US"));
  EXPECT_EQ(u"123 Main Street", profile.GetInfo(ADDRESS_HOME_LINE1, "en-US"));
  EXPECT_EQ(u"Springfield", profile.GetInfo(ADDRESS_HOME_CITY, "en-US"));
  EXPECT_EQ(u"IL", profile.GetInfo(ADDRESS_HOME_STATE, "en-US"));
  EXPECT_EQ(u"55123", profile.GetInfo(ADDRESS_HOME_ZIP, "en-US"));
}

void AutofillControllerTest::SetUpForSuggestions(
    NSString* data,
    size_t expected_number_of_forms) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"IL");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"55123");
  EXPECT_EQ(0U,
            personal_data_manager->address_data_manager().GetProfiles().size());
  PersonalDataChangedWaiter waiter(*personal_data_manager);
  personal_data_manager->address_data_manager().AddProfile(profile);
  std::move(waiter).Wait();
  EXPECT_EQ(1U,
            personal_data_manager->address_data_manager().GetProfiles().size());

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(data, expected_number_of_forms));
  web::test::WaitForBackgroundTasks();
}

// Checks that focusing on a text element of a profile-type form will result in
// suggestions being sent to the AutofillAgent, once data has been loaded into a
// test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestions) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/40064372): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.SuggestionsCount.Address", 1);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
}

// Tests that the system is able to offer suggestions for an anonymous form when
// there is another anonymous form on the page.
TEST_F(AutofillControllerTest, ProfileSuggestionsTwoAnonymousForms) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/40064372): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(
      [NSString stringWithFormat:@"%@%@", kProfileFormHtml, kProfileFormHtml],
      2);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.SuggestionsCount.Address", 1);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
}

// Checks that focusing on a select element in a profile-type form will result
// in suggestions being sent to the AutofillAgent, once data has been loaded
// into a test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestionsFromSelectField) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/40064372): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].state.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.SuggestionsCount.Address", 1);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"IL", suggestion.value);
}

// Checks that multiple profiles will offer a matching number of suggestions.
TEST_F(AutofillControllerTest, MultipleProfileSuggestions) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/40064372): Re-enable when fixed on iOS16.3+.
    return;
  }

  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  AutofillProfile profile(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"IL");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"55123");

  AutofillProfile profile2(
      autofill::i18n_model_definition::kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(NAME_FULL, u"Larry Page");
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, u"1600 Amphitheatre Parkway");
  profile2.SetRawInfo(ADDRESS_HOME_CITY, u"Mountain View");
  profile2.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, u"94043");

  EXPECT_EQ(0U,
            personal_data_manager->address_data_manager().GetProfiles().size());
  PersonalDataChangedWaiter waiter(*personal_data_manager);
  personal_data_manager->address_data_manager().AddProfile(profile);
  personal_data_manager->address_data_manager().AddProfile(profile2);
  std::move(waiter).Wait();
  EXPECT_EQ(2U,
            personal_data_manager->address_data_manager().GetProfiles().size());

  EXPECT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.SuggestionsCount.Address", 2);
  EXPECT_EQ(2U, [suggestion_controller() suggestions].count);
}

// Check that an HTML page containing a key/value type form which is submitted
// with scripts (simulating user form submission) results in data being
// successfully registered.
TEST_F(AutofillControllerTest, KeyValueImport) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kKeyValueFormHtml, 1));
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.value = 'Hello'",
                               web_state());
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  TestConsumer consumer;
  const int limit = 1;
  consumer.result_ = {CreateAutocompleteEntry(u"Should"),
                      CreateAutocompleteEntry(u"get"),
                      CreateAutocompleteEntry(u"overwritten")};
  web_data_service->GetFormValuesForElementName(u"greeting", std::u16string(),
                                                limit, &consumer);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  web::test::WaitForBackgroundTasks();
  // No value should be returned before anything is loaded via form submission.
  ASSERT_EQ(0U, consumer.result_.size());
  web::test::ExecuteJavaScript(@"submit.click()", web_state());
  // We can't make `consumer` a __block variable because TestConsumer lacks copy
  // construction. We just pass a pointer instead as we know that the callback
  // is executed within the life-cyle of `consumer`.
  TestConsumer* consumer_ptr = &consumer;
  WaitForCondition(^bool {
    web_data_service->GetFormValuesForElementName(u"greeting", std::u16string(),
                                                  limit, consumer_ptr);
    return consumer_ptr->result_.size();
  });
  base::ThreadPoolInstance::Get()->FlushForTesting();
  web::test::WaitForBackgroundTasks();
  // One result should be returned, matching the filled value.
  ASSERT_EQ(1U, consumer.result_.size());
  EXPECT_EQ(u"Hello", consumer.result_[0].key().value());
}

void AutofillControllerTest::SetUpKeyValueData() {
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  // Load value into database.
  std::vector<FormFieldData> values;
  FormFieldData fieldData;
  fieldData.set_name(u"greeting");
  fieldData.set_value(u"Bonjour");
  values.push_back(fieldData);
  web_data_service->AddFormFields(values);

  // Load test page.
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kKeyValueFormHtml, 1));
  web::test::WaitForBackgroundTasks();
}

// Checks that focusing on an element of a key/value type form then typing the
// first letter of a suggestion will result in suggestions being sent to the
// AutofillAgent, once data has been loaded into a test data manager.
TEST_F(AutofillControllerTest, KeyValueSuggestions) {
  SetUpKeyValueData();
  ResetWaitForSuggestionRetrieval();
  // Focus element.
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.value='B'",
                               web_state());
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.focus()",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that typing events (simulated in script) result in suggestions. Note
// that the field is not explicitly focused before typing starts; this can
// happen in practice and should not result in a crash or incorrect behavior.
TEST_F(AutofillControllerTest, KeyValueTypedSuggestions) {
  SetUpKeyValueData();
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.focus()",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ResetWaitForSuggestionRetrieval();
  SimulateTextInputEvent(/*field_id=*/@"greeting", /*field_value=*/@"B");

  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that focusing on and typing on one field, then changing focus before
// typing again, result in suggestions.
TEST_F(AutofillControllerTest, KeyValueFocusChange) {
  SetUpKeyValueData();

  // Focus the dummy field and confirm no suggestions are presented.
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].dummy.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ASSERT_EQ(0U, [suggestion_controller() suggestions].count);
  ResetWaitForSuggestionRetrieval();

  // Enter 'B' in the dummy field and confirm no suggestions are presented.
  SimulateTextInputEvent(/*field_id=*/@"dummy", /*field_value=*/@"B");

  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ASSERT_EQ(0U, [suggestion_controller() suggestions].count);
  ResetWaitForSuggestionRetrieval();

  // Enter 'B' in the greeting field and confirm that one suggestion ("Bonjour")
  // is presented.
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.focus()",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(
      @"var event = document.createEvent('TextEvent');", web_state());
  web::test::ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');",
      web_state());
  web::test::ExecuteJavaScript(
      @"document.forms[0].greeting.dispatchEvent(event);", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);

  ASSERT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that focusing on an element of a key/value type form without typing
// won't result in suggestions being sent to the AutofillAgent, once data has
// been loaded into a test data manager.
TEST_F(AutofillControllerTest, NoKeyValueSuggestionsWithoutTyping) {
  SetUpKeyValueData();
  ResetWaitForSuggestionRetrieval();
  // Focus element.
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.focus()",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);
}

// Checks that an HTML page containing a credit card-type form which is
// submitted with scripts (simulating user form submission) results in a credit
// card being successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, CreditCardImport) {
  InfoBarManagerImpl::CreateForWebState(web_state());
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  // Check there are no registered profiles already.
  EXPECT_EQ(
      0U,
      personal_data_manager->payments_data_manager().GetCreditCards().size());
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kCreditCardFormHtml, 1));
  web::test::ExecuteJavaScript(@"document.forms[0].name.value = 'Superman'",
                               web_state());
  web::test::ExecuteJavaScript(
      @"document.forms[0].CCNo.value = '4000-4444-4444-4444'", web_state());
  web::test::ExecuteJavaScript(@"document.forms[0].CCExpiresMonth.value = '11'",
                               web_state());
  web::test::ExecuteJavaScript(
      @"document.forms[0].CCExpiresYear.value = '2999'", web_state());
  web::test::ExecuteJavaScript(@"submit.click()", web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  WaitForCondition(^bool() {
    return infobar_manager->infobars().size();
  });
  ExpectMetric("Autofill.CreditCardInfoBar.Local",
               AutofillMetrics::INFOBAR_SHOWN);
  ASSERT_EQ(1U, infobar_manager->infobars().size());
  infobars::InfoBarDelegate* infobar =
      infobar_manager->infobars()[0]->delegate();
  ConfirmInfoBarDelegate* confirm_infobar = infobar->AsConfirmInfoBarDelegate();

  // This call cause a modification of the PersonalDataManager, so wait until
  // the asynchronous task complete in addition to waiting for the UI update.
  PersonalDataChangedWaiter waiter(*personal_data_manager);
  confirm_infobar->Accept();
  std::move(waiter).Wait();

  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];
  EXPECT_EQ(u"Superman", credit_card.GetInfo(CREDIT_CARD_NAME_FULL, "en-US"));
  EXPECT_EQ(u"4000444444444444",
            credit_card.GetInfo(CREDIT_CARD_NUMBER, "en-US"));
  EXPECT_EQ(u"11", credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, "en-US"));
  EXPECT_EQ(u"2999",
            credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, "en-US"));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::FORM_SUBMISSION,
      /*expected_count=*/1);
}

// Checks that an HTML page containing a credit card-type form which is
// submitted with scripts (simulating form removal) results in a credit
// card being successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, CreditCardImportAfterFormRemoval) {
  InfoBarManagerImpl::CreateForWebState(web_state());
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  // Check there are no registered profiles already.
  EXPECT_EQ(
      0U,
      personal_data_manager->payments_data_manager().GetCreditCards().size());

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kCreditCardFormHtml, 1));

  // Simulate entering a credit card in the form.
  SimulateTextInputEvent(/*field_id=*/@"name", /*field_value=*/@"Superman");
  SimulateTextInputEvent(/*field_id=*/@"CCNo",
                         /*field_value=*/@"4000-4444-4444-4444");
  SimulateTextInputEvent(/*field_id=*/@"CCExpiresMonth", /*field_value=*/@"11");
  SimulateTextInputEvent(/*field_id=*/@"CCExpiresYear",
                         /*field_value=*/@"2999");

  // Deleting the form should be detected as a submission because it had user
  // input. Adding a delay is necessary or the event above might not be
  // dispatched.
  web::test::ExecuteJavaScript(@"setTimeout(function(){"
                               @"   document.forms[0].remove();"
                               @"}, 30);",
                               web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  WaitForCondition(^bool() {
    return infobar_manager->infobars().size();
  });
  ExpectMetric("Autofill.CreditCardInfoBar.Local",
               AutofillMetrics::INFOBAR_SHOWN);
  ASSERT_EQ(1U, infobar_manager->infobars().size());
  infobars::InfoBarDelegate* infobar =
      infobar_manager->infobars()[0]->delegate();
  ConfirmInfoBarDelegate* confirm_infobar = infobar->AsConfirmInfoBarDelegate();

  // This call cause a modification of the PersonalDataManager, so wait until
  // the asynchronous task complete in addition to waiting for the UI update.
  PersonalDataChangedWaiter waiter(*personal_data_manager);
  confirm_infobar->Accept();
  std::move(waiter).Wait();

  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];
  EXPECT_EQ(u"Superman", credit_card.GetInfo(CREDIT_CARD_NAME_FULL, "en-US"));
  EXPECT_EQ(u"4000444444444444",
            credit_card.GetInfo(CREDIT_CARD_NUMBER, "en-US"));
  EXPECT_EQ(u"11", credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, "en-US"));
  EXPECT_EQ(u"2999",
            credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, "en-US"));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::XHR_SUCCEEDED,
      /*expected_count=*/1);
}

// Checks that an HTML page containing a credit card-type form which is
// submitted with scripts (simulating form removal) results in a credit
// card being successfully imported into the PersonalDataManager. The test
// verifies that the imported card includes the lastest known field values for
// the submitted form.
TEST_F(AutofillControllerTest,
       CreditCardImportWithFieldDataManagerValuesAfterFormRemoval) {
  InfoBarManagerImpl::CreateForWebState(web_state());
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  // Check there are no registered profiles already.
  EXPECT_EQ(
      0U,
      personal_data_manager->payments_data_manager().GetCreditCards().size());

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kCreditCardFormHtml, 1));

  // Simulate entering a credit card in the form.
  SimulateTextInputEvent(/*field_id=*/@"name", /*field_value=*/@"Superman");
  SimulateTextInputEvent(/*field_id=*/@"CCNo",
                         /*field_value=*/@"4000-4444-4444-4444");
  SimulateTextInputEvent(/*field_id=*/@"CCExpiresMonth", /*field_value=*/@"11");
  SimulateTextInputEvent(/*field_id=*/@"CCExpiresYear",
                         /*field_value=*/@"2999");

  // Update the form fields in `FieldDataManager`.
  // When detecting a submission, the imported credit card should include the
  // latest values in `FieldDataManager`.
  auto* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());
  auto* main_frame = frames_manager->GetMainWebFrame();
  auto* fieldDataManager =
      autofill::FieldDataManagerFactoryIOS::FromWebFrame(main_frame);
  // Name.
  fieldDataManager->UpdateFieldDataMap(FieldRendererId(2), u"Chuck",
                                       FieldPropertiesFlags::kAutofilled);
  // CCNo.
  fieldDataManager->UpdateFieldDataMap(FieldRendererId(3),
                                       u"5425-2334-3010-9903",
                                       FieldPropertiesFlags::kAutofilled);
  // CCExpiresMonth.
  fieldDataManager->UpdateFieldDataMap(FieldRendererId(4), u"12",
                                       FieldPropertiesFlags::kAutofilled);
  // CCExpiresYear.
  fieldDataManager->UpdateFieldDataMap(FieldRendererId(5), u"2998",
                                       FieldPropertiesFlags::kAutofilled);

  // Deleting the form should be detected as a submission because it had user
  // input. Adding a delay is necessary or the event above might not be
  // dispatched.
  web::test::ExecuteJavaScript(@"setTimeout(function(){"
                               @"   document.forms[0].remove();"
                               @"}, 30);",
                               web_state());

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  WaitForCondition(^bool() {
    return infobar_manager->infobars().size();
  });
  ExpectMetric("Autofill.CreditCardInfoBar.Local",
               AutofillMetrics::INFOBAR_SHOWN);
  ASSERT_EQ(1U, infobar_manager->infobars().size());
  infobars::InfoBarDelegate* infobar =
      infobar_manager->infobars()[0]->delegate();
  ConfirmInfoBarDelegate* confirm_infobar = infobar->AsConfirmInfoBarDelegate();

  // This call cause a modification of the PersonalDataManager, so wait until
  // the asynchronous task complete in addition to waiting for the UI update.
  PersonalDataChangedWaiter waiter(*personal_data_manager);
  confirm_infobar->Accept();
  std::move(waiter).Wait();

  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];

  EXPECT_EQ(u"Chuck", credit_card.GetInfo(CREDIT_CARD_NAME_FULL, "en-US"));
  EXPECT_EQ(u"5425233430109903",
            credit_card.GetInfo(CREDIT_CARD_NUMBER, "en-US"));
  EXPECT_EQ(u"12", credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, "en-US"));
  EXPECT_EQ(u"2998",
            credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, "en-US"));
}

// Checks that an HTML page containing a profile-type formless form which is
// submitted with scripts (simulating form removal) results in a profile being
// successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, ProfileImportAfterFormlessFormRemoval) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(profile_.get()));
  test_api(personal_data_manager->address_data_manager())
      .set_auto_accept_address_imports(true);
  // Check there are no registered profiles already.
  EXPECT_EQ(0U,
            personal_data_manager->address_data_manager().GetProfiles().size());
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormlessHtml, 1));

  // Simulate entering a profile in the fields.
  SimulateTextInputEvent(/*field_id=*/@"name",
                         /*field_value=*/@"Homer Simpson");
  SimulateTextInputEvent(/*field_id=*/@"address",
                         /*field_value=*/@"123 Main Street");
  SimulateTextInputEvent(/*field_id=*/@"city", /*field_value=*/@"Springfield");
  SimulateTextInputEvent(/*field_id=*/@"state", /*field_value=*/@"IL");
  SimulateTextInputEvent(/*field_id=*/@"zip", /*field_value=*/@"55123");

  // Deleting the form should be detected as a submission because it had user
  // input. Adding a delay is necessary or the event above might not be
  // dispatched.
  web::test::ExecuteJavaScript(@"setTimeout(function(){"
                               @"   document.getElementById('div').remove();"
                               @"}, 30);",
                               web_state());

  WaitForCondition(^bool {
    return personal_data_manager->address_data_manager().GetProfiles().size();
  });
  const std::vector<const AutofillProfile*>& profiles =
      personal_data_manager->address_data_manager().GetProfiles();
  if (profiles.size() != 1) {
    FAIL() << "Not exactly one profile found after attempted import";
  }
  const AutofillProfile& profile = *profiles[0];
  EXPECT_EQ(u"Homer Simpson", profile.GetInfo(NAME_FULL, "en-US"));
  EXPECT_EQ(u"123 Main Street", profile.GetInfo(ADDRESS_HOME_LINE1, "en-US"));
  EXPECT_EQ(u"Springfield", profile.GetInfo(ADDRESS_HOME_CITY, "en-US"));
  EXPECT_EQ(u"IL", profile.GetInfo(ADDRESS_HOME_STATE, "en-US"));
  EXPECT_EQ(u"55123", profile.GetInfo(ADDRESS_HOME_ZIP, "en-US"));

  histogram_tester_->ExpectUniqueSample(
      /*name=*/kAutofillSubmissionDetectionSourceHistogram,
      /*sample=*/mojom::SubmissionSource::XHR_SUCCEEDED,
      /*expected_count=*/1);
}

}  // namespace

}  // namespace autofill
