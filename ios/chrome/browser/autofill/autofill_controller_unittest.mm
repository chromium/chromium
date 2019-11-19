// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#import <UIKit/UIKit.h>

#include "base/guid.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/common/autofill_clock.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#include "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface TestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;
@property(nonatomic, assign) BOOL suggestionRetrievalComplete;

@end

@implementation TestSuggestionController

@synthesize suggestions = _suggestions;
@synthesize suggestionRetrievalComplete = _suggestionRetrievalComplete;

- (void)retrieveSuggestionsForForm:(const autofill::FormActivityParams&)params
                          webState:(web::WebState*)webState {
  self.suggestionRetrievalComplete = NO;
  [super retrieveSuggestionsForForm:params webState:webState];
}

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
  self.suggestionRetrievalComplete = YES;
}

- (void)onNoSuggestionsAvailable {
  self.suggestionRetrievalComplete = YES;
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
     "Greeting <input type='text' name='greeting'>"
     "Dummy field <input type='text' name='dummy'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// The credit card-type form used by tests.
NSString* const kCreditCardFormHtml =
    @"<form action='/submit' method='post'>"
     "Name on card: <input type='text' name='name'>"
     "Credit card number: <input type='text' name='CCNo'>"
     "Expiry Month: <input type='text' name='CCExpiresMonth'>"
     "Expiry Year: <input type='text' name='CCExpiresYear'>"
     "<input type='submit' id='submit' value='Submit'>"
     "</form>";

// An HTML page without a card-type form.
static NSString* kNoCreditCardFormHtml =
    @"<form><input type=\"text\" autofocus autocomplete=\"username\"></form>";

// A credit card-type form with the autofocus attribute (which is detected at
// page load).
NSString* const kCreditCardAutofocusFormHtml =
    @"<form><input type=\"text\" autofocus autocomplete=\"cc-number\"></form>";

// FAIL if a field with the supplied |name| and |fieldType| is not present on
// the |form|.
void CheckField(const FormStructure& form,
                ServerFieldType fieldType,
                const char* name) {
  for (const auto& field : form) {
    if (field->heuristic_type() == fieldType) {
      EXPECT_EQ(base::UTF8ToUTF16(name), field->unique_name());
      return;
    }
  }
  FAIL() << "Missing field " << name;
}

AutofillEntry CreateAutofillEntry(const base::string16& value) {
  const base::Time kNow = AutofillClock::Now();
  return AutofillEntry(AutofillKey(base::ASCIIToUTF16("Name"), value), kNow,
                       kNow);
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

// WebDataServiceConsumer for receiving vectors of strings and making them
// available to tests.
class TestConsumer : public WebDataServiceConsumer {
 public:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK_EQ(result->GetType(), AUTOFILL_VALUE_RESULT);
    result_ = static_cast<WDResult<std::vector<AutofillEntry>>*>(result.get())
                  ->GetValue();
  }
  std::vector<AutofillEntry> result_;
};

// Text fixture to test autofill.
class AutofillControllerTest : public ChromeWebTest {
 public:
  AutofillControllerTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()) {}
  ~AutofillControllerTest() override {}

 protected:
  void SetUp() override;
  void TearDown() override;

  void SetUpForSuggestions(NSString* data, size_t expected_number_of_forms);

  // Adds key value data to the Personal Data Manager and loads test page.
  void SetUpKeyValueData();

  // Blocks until suggestion retrieval has completed.
  // If |wait_for_trigger| is yes, wait for the call to
  // |retrieveSuggestionsForForm| to avoid considering a former call.
  void WaitForSuggestionRetrieval(BOOL wait_for_trigger);

  // Blocks until |expected_size| forms have been fecthed.
  bool WaitForFormFetched(AutofillManager* manager,
                          size_t expected_number_of_forms) WARN_UNUSED_RESULT;

  // Loads the page and wait until the initial form processing has been done.
  // This processing must find |expected_size| forms.
  bool LoadHtmlAndWaitForFormFetched(NSString* html,
                                     size_t expected_number_of_forms)
      WARN_UNUSED_RESULT;

  // Fails if the specified metric was not registered the given number of times.
  void ExpectMetric(const std::string& histogram_name, int sum);

  // Fails if the specified user happiness metric was not registered.
  void ExpectHappinessMetric(AutofillMetrics::UserHappinessMetric metric);

  TestSuggestionController* suggestion_controller() {
    return suggestion_controller_;
  }

 private:
  // Histogram tester for these tests.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;

  AutofillAgent* autofill_agent_;

  // Retrieves suggestions according to form events.
  TestSuggestionController* suggestion_controller_;

  // Retrieves accessory views according to form events.
  FormInputAccessoryMediator* accessory_mediator_;

  DISALLOW_COPY_AND_ASSIGN(AutofillControllerTest);
};

void AutofillControllerTest::SetUp() {
  ChromeWebTest::SetUp();

  // Profile import requires a PersonalDataManager which itself needs the
  // WebDataService; this is not initialized on a TestChromeBrowserState by
  // default.
  chrome_browser_state_->CreateWebDataService();

  autofill_agent_ = [[AutofillAgent alloc]
      initWithPrefService:chrome_browser_state_->GetPrefs()
                 webState:web_state()];
  suggestion_controller_ =
      [[TestSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofill_agent_ ]];

  InfoBarManagerImpl::CreateForWebState(web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  autofill_client_.reset(new autofill::ChromeAutofillClientIOS(
      chrome_browser_state_.get(), web_state(), infobar_manager,
      autofill_agent_,
      /*password_generation_manager=*/nullptr));

  std::string locale("en");
  autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
      web_state(), autofill_client_.get(), /*autofill_agent=*/nil, locale,
      autofill::AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);

  accessory_mediator_ =
      [[FormInputAccessoryMediator alloc] initWithConsumer:nil
                                                  delegate:nil
                                              webStateList:NULL
                                       personalDataManager:NULL
                                             passwordStore:nullptr];

  [accessory_mediator_ injectWebState:web_state()];
  [accessory_mediator_ injectProvider:suggestion_controller_];
  auto suggestionManager = base::mac::ObjCCastStrict<JsSuggestionManager>(
      [web_state()->GetJSInjectionReceiver()
          instanceOfClass:[JsSuggestionManager class]]);
  [accessory_mediator_ injectSuggestionManager:suggestionManager];

  histogram_tester_.reset(new base::HistogramTester());
}

void AutofillControllerTest::TearDown() {
  [suggestion_controller_ detachFromWebState];

  ChromeWebTest::TearDown();
}

void AutofillControllerTest::WaitForSuggestionRetrieval(BOOL wait_for_trigger) {
  // Wait for the message queue to ensure that JS events fired in the tests
  // trigger TestSuggestionController's retrieveSuggestionsForFormNamed: method
  // and set suggestionRetrievalComplete to NO.
  if (wait_for_trigger) {
    WaitForCondition(^bool {
      return ![suggestion_controller() suggestionRetrievalComplete];
    });
  }
  // Now we can wait for suggestionRetrievalComplete to be set to YES.
  WaitForCondition(^bool {
    return [suggestion_controller() suggestionRetrievalComplete];
  });
}

bool AutofillControllerTest::WaitForFormFetched(
    AutofillManager* manager,
    size_t expected_number_of_forms) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return manager->form_structures().size() == expected_number_of_forms;
      });
}

bool AutofillControllerTest::LoadHtmlAndWaitForFormFetched(
    NSString* html,
    size_t expected_number_of_forms) {
  LoadHtml(html);
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
          ->autofill_manager();
  return WaitForFormFetched(autofill_manager, expected_number_of_forms);
}

void AutofillControllerTest::ExpectMetric(const std::string& histogram_name,
                                          int sum) {
  histogram_tester_->ExpectBucketCount(histogram_name, sum, 1);
}

void AutofillControllerTest::ExpectHappinessMetric(
    AutofillMetrics::UserHappinessMetric metric) {
  histogram_tester_->ExpectBucketCount("Autofill.UserHappiness", metric, 1);
}

// Checks that viewing an HTML page containing a form results in the form being
// registered as a FormStructure by the AutofillManager.
TEST_F(AutofillControllerTest, ReadForm) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
          ->autofill_manager();
  const auto& forms = autofill_manager->form_structures();
  const auto& form = *(forms.begin()->second);
  CheckField(form, NAME_FULL, "name_1");
  CheckField(form, ADDRESS_HOME_LINE1, "address_1");
  CheckField(form, ADDRESS_HOME_CITY, "city_1");
  CheckField(form, ADDRESS_HOME_STATE, "state_1");
  CheckField(form, ADDRESS_HOME_ZIP, "zip_1");
  ExpectMetric("Autofill.IsEnabled.PageLoad", 1);
  ExpectHappinessMetric(AutofillMetrics::FORMS_LOADED);
}

// Checks that viewing an HTML page containing a form with an 'id' results in
// the form being registered as a FormStructure by the AutofillManager, and the
// name is correctly set.
TEST_F(AutofillControllerTest, ReadFormName) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kMinimalFormWithNameHtml, 1));
  web::WebFrame* main_frame =
      web_state()->GetWebFramesManager()->GetMainWebFrame();
  AutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), main_frame)
          ->autofill_manager();
  const auto& forms = autofill_manager->form_structures();
  const auto& form = *(forms.begin()->second);
  EXPECT_EQ(base::UTF8ToUTF16("form1"), form.ToFormData().name);
}

// Checks that an HTML page containing a profile-type form which is submitted
// with scripts (simulating user form submission) results in a profile being
// successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, ProfileImport) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(GetBrowserState()));
  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  ExecuteJavaScript(@"document.forms[0].name.value = 'Homer Simpson'");
  ExecuteJavaScript(@"document.forms[0].address.value = '123 Main Street'");
  ExecuteJavaScript(@"document.forms[0].city.value = 'Springfield'");
  ExecuteJavaScript(@"document.forms[0].state.value = 'IL'");
  ExecuteJavaScript(@"document.forms[0].zip.value = '55123'");
  ExecuteJavaScript(@"submit.click()");
  WaitForCondition(^bool {
    return personal_data_manager->GetProfiles().size();
  });
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager->GetProfiles();
  if (profiles.size() != 1)
    FAIL() << "Not exactly one profile found after attempted import";
  const AutofillProfile& profile = *profiles[0];
  EXPECT_EQ(base::UTF8ToUTF16("Homer Simpson"),
            profile.GetInfo(AutofillType(NAME_FULL), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("123 Main Street"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_LINE1), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("Springfield"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_CITY), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("IL"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_STATE), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("55123"),
            profile.GetInfo(AutofillType(ADDRESS_HOME_ZIP), "en-US"));
}

void AutofillControllerTest::SetUpForSuggestions(
    NSString* data,
    size_t expected_number_of_forms) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(GetBrowserState()));
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  profile.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Homer Simpson"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, base::UTF8ToUTF16("123 Main Street"));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Springfield"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("IL"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("55123"));
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  personal_data_manager->SaveImportedProfile(profile);
  EXPECT_EQ(1U, personal_data_manager->GetProfiles().size());

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(data, expected_number_of_forms));
  WaitForBackgroundTasks();
}

// Checks that focusing on a text element of a profile-type form will result in
// suggestions being sent to the AutofillAgent, once data has been loaded into a
// test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestions) {
  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
}

// Tests that the system is able to offer suggestions for an anonymous form when
// there is another anonymous form on the page.
TEST_F(AutofillControllerTest, ProfileSuggestionsTwoAnonymousForms) {
  SetUpForSuggestions(
      [NSString stringWithFormat:@"%@%@", kProfileFormHtml, kProfileFormHtml],
      2);
  ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Homer Simpson", suggestion.value);
}

// Checks that focusing on a select element in a profile-type form will result
// in suggestions being sent to the AutofillAgent, once data has been loaded
// into a test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestionsFromSelectField) {
  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].state.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"IL", suggestion.value);
}

// Checks that multiple profiles will offer a matching number of suggestions.
TEST_F(AutofillControllerTest, MultipleProfileSuggestions) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(GetBrowserState()));
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);

  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  profile.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Homer Simpson"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, base::UTF8ToUTF16("123 Main Street"));
  profile.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Springfield"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("IL"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("55123"));
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());

  personal_data_manager->SaveImportedProfile(profile);
  waiter.Wait();

  EXPECT_EQ(1U, personal_data_manager->GetProfiles().size());
  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com/");
  profile2.SetRawInfo(NAME_FULL, base::UTF8ToUTF16("Larry Page"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1,
                      base::UTF8ToUTF16("1600 Amphitheatre Parkway"));
  profile2.SetRawInfo(ADDRESS_HOME_CITY, base::UTF8ToUTF16("Mountain View"));
  profile2.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("CA"));
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16("94043"));
  personal_data_manager->SaveImportedProfile(profile2);
  EXPECT_EQ(2U, personal_data_manager->GetProfiles().size());
  EXPECT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  ForceViewRendering(web_state()->GetView());
  ExecuteJavaScript(@"document.forms[0].name.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.AddressSuggestionsCount", 2);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(2U, [suggestion_controller() suggestions].count);
}

// Check that an HTML page containing a key/value type form which is submitted
// with scripts (simulating user form submission) results in data being
// successfully registered.
TEST_F(AutofillControllerTest, KeyValueImport) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kKeyValueFormHtml, 1));
  ExecuteJavaScript(@"document.forms[0].greeting.value = 'Hello'");
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  __block TestConsumer consumer;
  const int limit = 1;
  consumer.result_ = {CreateAutofillEntry(base::ASCIIToUTF16("Should")),
                      CreateAutofillEntry(base::ASCIIToUTF16("get")),
                      CreateAutofillEntry(base::ASCIIToUTF16("overwritten"))};
  web_data_service->GetFormValuesForElementName(
      base::UTF8ToUTF16("greeting"), base::string16(), limit, &consumer);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  WaitForBackgroundTasks();
  // No value should be returned before anything is loaded via form submission.
  ASSERT_EQ(0U, consumer.result_.size());
  ExecuteJavaScript(@"submit.click()");
  WaitForCondition(^bool {
    web_data_service->GetFormValuesForElementName(
        base::UTF8ToUTF16("greeting"), base::string16(), limit, &consumer);
    return consumer.result_.size();
  });
  base::ThreadPoolInstance::Get()->FlushForTesting();
  WaitForBackgroundTasks();
  // One result should be returned, matching the filled value.
  ASSERT_EQ(1U, consumer.result_.size());
  EXPECT_EQ(base::UTF8ToUTF16("Hello"), consumer.result_[0].key().value());
}

void AutofillControllerTest::SetUpKeyValueData() {
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  // Load value into database.
  std::vector<FormFieldData> values;
  FormFieldData fieldData;
  fieldData.name = base::UTF8ToUTF16("greeting");
  fieldData.value = base::UTF8ToUTF16("Bonjour");
  values.push_back(fieldData);
  web_data_service->AddFormFields(values);

  // Load test page.
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kKeyValueFormHtml, 1));
  WaitForBackgroundTasks();
}

// Checks that focusing on an element of a key/value type form then typing the
// first letter of a suggestion will result in suggestions being sent to the
// AutofillAgent, once data has been loaded into a test data manager.
TEST_F(AutofillControllerTest, KeyValueSuggestions) {
  SetUpKeyValueData();

  // Focus element.
  ExecuteJavaScript(@"document.forms[0].greeting.value='B'");
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
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
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].greeting.dispatchEvent(event);");
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
  ExecuteJavaScript(@"document.forms[0].dummy.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);

  // Enter 'B' in the dummy field and confirm no suggestions are presented.
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].dummy.dispatchEvent(event);");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);

  // Enter 'B' in the greeting field and confirm that one suggestion ("Bonjour")
  // is presented.
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExecuteJavaScript(@"event = document.createEvent('TextEvent');");
  ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');");
  ExecuteJavaScript(@"document.forms[0].greeting.dispatchEvent(event);");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"Bonjour", suggestion.value);
}

// Checks that focusing on an element of a key/value type form without typing
// won't result in suggestions being sent to the AutofillAgent, once data has
// been loaded into a test data manager.
TEST_F(AutofillControllerTest, NoKeyValueSuggestionsWithoutTyping) {
  SetUpKeyValueData();

  // Focus element.
  ExecuteJavaScript(@"document.forms[0].greeting.focus()");
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  EXPECT_EQ(0U, [suggestion_controller() suggestions].count);
}

// Checks that an HTML page containing a credit card-type form which is
// submitted with scripts (simulating user form submission) results in a credit
// card being successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, CreditCardImport) {
  InfoBarManagerImpl::CreateForWebState(web_state());
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(GetBrowserState()));

  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetCreditCards().size());
  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(kCreditCardFormHtml, 1));
  ExecuteJavaScript(@"document.forms[0].name.value = 'Superman'");
  ExecuteJavaScript(@"document.forms[0].CCNo.value = '4000-4444-4444-4444'");
  ExecuteJavaScript(@"document.forms[0].CCExpiresMonth.value = '11'");
  ExecuteJavaScript(@"document.forms[0].CCExpiresYear.value = '2999'");
  ExecuteJavaScript(@"submit.click()");
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  WaitForCondition(^bool() {
    return infobar_manager->infobar_count();
  });
  ExpectMetric("Autofill.CreditCardInfoBar.Local",
               AutofillMetrics::INFOBAR_SHOWN);
  ASSERT_EQ(1U, infobar_manager->infobar_count());
  infobars::InfoBarDelegate* infobar =
      infobar_manager->infobar_at(0)->delegate();
  ConfirmInfoBarDelegate* confirm_infobar = infobar->AsConfirmInfoBarDelegate();

  // This call cause a modification of the PersonalDataManager, so wait until
  // the asynchronous task complete in addition to waiting for the UI update.
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);
  confirm_infobar->Accept();
  waiter.Wait();

  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];
  EXPECT_EQ(base::UTF8ToUTF16("Superman"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("4000444444444444"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), "en-US"));
  EXPECT_EQ(base::UTF8ToUTF16("11"),
            credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), "en-US"));
  EXPECT_EQ(
      base::UTF8ToUTF16("2999"),
      credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), "en-US"));
}

}  // namespace

}  // namespace autofill
