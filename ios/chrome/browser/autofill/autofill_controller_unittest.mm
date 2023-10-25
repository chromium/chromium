// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <vector>

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/browser/webdata/autocomplete_entry.h"
#import "components/autofill/core/common/autofill_clock.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
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

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

// FAIL if a field with the supplied `name` and `fieldType` is not present on
// the `form`.
void CheckField(const FormStructure& form,
                ServerFieldType fieldType,
                const char* name) {
  for (const auto& field : form) {
    if (field->heuristic_type() == fieldType) {
      EXPECT_EQ(base::UTF8ToUTF16(name), field->name);
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
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    // Profile import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    web::WebState::CreateParams params(browser_state_.get());
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
    AutofillDownloadManager* GetDownloadManager() override { return nullptr; }
  };

  class TestAutofillManager : public BrowserAutofillManager {
   public:
    TestAutofillManager(AutofillDriverIOS* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}

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
      size_t expected_number_of_forms);

  void LoadHtmlAndInitRendererIds(NSString* html);

  // Fails if the specified metric was not registered the given number of times.
  void ExpectMetric(const std::string& histogram_name, int sum);

  // Fails if the specified user happiness metric was not registered.
  void ExpectHappinessMetric(AutofillMetrics::UserHappinessMetric metric);

  TestSuggestionController* suggestion_controller() {
    return suggestion_controller_;
  }

  void WaitForCondition(ConditionBlock condition);

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  bool processed_a_task_ = false;

 private:
  // Histogram tester for these tests.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

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
  UniqueIDDataTabHelper::CreateForWebState(web_state());
  passwordController_ =
      [[PasswordController alloc] initWithWebState:web_state()];

  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                        webState:web_state()];
  suggestion_controller_ =
      [[TestSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofill_agent_ ]];

  InfoBarManagerImpl::CreateForWebState(web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  autofill_client_ = std::make_unique<TestAutofillClient>(
      browser_state_.get(), web_state(), infobar_manager, autofill_agent_);

  autofill_client_->GetPersonalDataManager()
      ->personal_data_manager_cleaner_for_testing()
      ->alternative_state_name_map_updater_for_testing()
      ->set_local_state_for_testing(local_state_.Get());

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
                                    reauthenticationModule:nil];

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
    size_t expected_number_of_forms) {
  web::test::LoadHtml(html, web_state());
  TestAutofillManager* autofill_manager =
      autofill_manager_injector_->GetForMainFrame();
  return autofill_manager->waiter().Wait(1) &&
         autofill_manager->form_structures().size() == expected_number_of_forms;
}

void AutofillControllerTest::ExpectMetric(const std::string& histogram_name,
                                          int sum) {
  histogram_tester_->ExpectBucketCount(histogram_name, sum, 1);
}

void AutofillControllerTest::ExpectHappinessMetric(
    AutofillMetrics::UserHappinessMetric metric) {
  histogram_tester_->ExpectBucketCount("Autofill.UserHappiness", metric, 1);
}

void AutofillControllerTest::WaitForCondition(ConditionBlock condition) {
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1000),
                                                           true, condition));
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
  ExpectHappinessMetric(AutofillMetrics::FORMS_LOADED);
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
  EXPECT_EQ(u"form1", form.ToFormData().name);
}

// Checks that an HTML page containing a profile-type form which is submitted
// with scripts (simulating user form submission) results in a profile being
// successfully imported into the PersonalDataManager.
TEST_F(AutofillControllerTest, ProfileImport) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(browser_state_.get()));
  personal_data_manager->set_auto_accept_address_imports_for_testing(true);
  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
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
    return personal_data_manager->GetProfiles().size();
  });
  const std::vector<AutofillProfile*>& profiles =
      personal_data_manager->GetProfiles();
  if (profiles.size() != 1)
    FAIL() << "Not exactly one profile found after attempted import";
  const AutofillProfile& profile = *profiles[0];
  EXPECT_EQ(u"Homer Simpson",
            profile.GetInfo(AutofillType(NAME_FULL), "en-US"));
  EXPECT_EQ(u"123 Main Street",
            profile.GetInfo(AutofillType(ADDRESS_HOME_LINE1), "en-US"));
  EXPECT_EQ(u"Springfield",
            profile.GetInfo(AutofillType(ADDRESS_HOME_CITY), "en-US"));
  EXPECT_EQ(u"IL", profile.GetInfo(AutofillType(ADDRESS_HOME_STATE), "en-US"));
  EXPECT_EQ(u"55123", profile.GetInfo(AutofillType(ADDRESS_HOME_ZIP), "en-US"));
}

void AutofillControllerTest::SetUpForSuggestions(
    NSString* data,
    size_t expected_number_of_forms) {
  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(browser_state_.get()));
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"IL");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"55123");
  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);
  personal_data_manager->AddProfile(profile);
  waiter.Wait();
  EXPECT_EQ(1U, personal_data_manager->GetProfiles().size());

  ASSERT_TRUE(LoadHtmlAndWaitForFormFetched(data, expected_number_of_forms));
  web::test::WaitForBackgroundTasks();
}

// Checks that focusing on a text element of a profile-type form will result in
// suggestions being sent to the AutofillAgent, once data has been loaded into a
// test data manager.
TEST_F(AutofillControllerTest, ProfileSuggestions) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/1442607): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
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
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/1442607): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(
      [NSString stringWithFormat:@"%@%@", kProfileFormHtml, kProfileFormHtml],
      2);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
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
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/1442607): Re-enable when fixed on iOS16.3+.
    return;
  }

  SetUpForSuggestions(kProfileFormHtml, 1);
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].state.focus()", web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ExpectMetric("Autofill.AddressSuggestionsCount", 1);
  ExpectHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);
  EXPECT_EQ(1U, [suggestion_controller() suggestions].count);
  FormSuggestion* suggestion = [suggestion_controller() suggestions][0];
  EXPECT_NSEQ(@"IL", suggestion.value);
}

// Checks that multiple profiles will offer a matching number of suggestions.
TEST_F(AutofillControllerTest, MultipleProfileSuggestions) {
  if (@available(iOS 16.3, *)) {
    // TODO(crbug.com/1442607): Re-enable when fixed on iOS16.3+.
    return;
  }

  PersonalDataManager* personal_data_manager =
      PersonalDataManagerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(browser_state_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"IL");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"55123");

  AutofillProfile profile2;
  profile2.SetRawInfo(NAME_FULL, u"Larry Page");
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, u"1600 Amphitheatre Parkway");
  profile2.SetRawInfo(ADDRESS_HOME_CITY, u"Mountain View");
  profile2.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile2.SetRawInfo(ADDRESS_HOME_ZIP, u"94043");

  EXPECT_EQ(0U, personal_data_manager->GetProfiles().size());
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);
  personal_data_manager->AddProfile(profile);
  personal_data_manager->AddProfile(profile2);
  waiter.Wait();
  EXPECT_EQ(2U, personal_data_manager->GetProfiles().size());

  EXPECT_TRUE(LoadHtmlAndWaitForFormFetched(kProfileFormHtml, 1));
  ForceViewRendering(web_state()->GetView());
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"document.forms[0].name.focus()", web_state());
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
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.value = 'Hello'",
                               web_state());
  scoped_refptr<AutofillWebDataService> web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
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
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  // Load value into database.
  std::vector<FormFieldData> values;
  FormFieldData fieldData;
  fieldData.name = u"greeting";
  fieldData.value = u"Bonjour";
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
  web::test::ExecuteJavaScript(@"event = document.createEvent('TextEvent');",
                               web_state());
  web::test::ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');",
      web_state());
  web::test::ExecuteJavaScript(
      @"document.forms[0].greeting.dispatchEvent(event);", web_state());
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
  web::test::ExecuteJavaScript(@"event = document.createEvent('TextEvent');",
                               web_state());
  web::test::ExecuteJavaScript(
      @"event.initTextEvent('textInput', true, true, window, 'B');",
      web_state());

  web::test::ExecuteJavaScript(@"document.forms[0].dummy.dispatchEvent(event);",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ASSERT_EQ(0U, [suggestion_controller() suggestions].count);
  ResetWaitForSuggestionRetrieval();

  // Enter 'B' in the greeting field and confirm that one suggestion ("Bonjour")
  // is presented.
  web::test::ExecuteJavaScript(@"document.forms[0].greeting.focus()",
                               web_state());
  WaitForSuggestionRetrieval(/*wait_for_trigger=*/YES);
  ResetWaitForSuggestionRetrieval();
  web::test::ExecuteJavaScript(@"event = document.createEvent('TextEvent');",
                               web_state());
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
      PersonalDataManagerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(browser_state_.get()));
  personal_data_manager->SetSyncServiceForTest(nullptr);

  // Check there are no registered profiles already.
  EXPECT_EQ(0U, personal_data_manager->GetCreditCards().size());
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
  PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);
  confirm_infobar->Accept();
  waiter.Wait();

  const std::vector<CreditCard*>& credit_cards =
      personal_data_manager->GetCreditCards();
  ASSERT_EQ(1U, credit_cards.size());
  const CreditCard& credit_card = *credit_cards[0];
  EXPECT_EQ(u"Superman",
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), "en-US"));
  EXPECT_EQ(u"4000444444444444",
            credit_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), "en-US"));
  EXPECT_EQ(u"11",
            credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), "en-US"));
  EXPECT_EQ(u"2999", credit_card.GetInfo(
                         AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), "en-US"));
}

}  // namespace

}  // namespace autofill
