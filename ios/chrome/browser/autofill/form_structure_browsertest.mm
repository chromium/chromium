// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <WebKit/WebKit.h>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#include "testing/data_driven_testing/data_driven_test.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace autofill {

namespace {

const base::FilePath::CharType kFeatureName[] = FILE_PATH_LITERAL("autofill");
const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");

base::FilePath GetTestDataDir() {
  base::FilePath dir;
  base::PathService::Get(ios::DIR_TEST_DATA, &dir);
  return dir;
}

base::FilePath GetIOSInputDirectory() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .Append(kFeatureName)
      .Append(kTestName)
      .AppendASCII("input");
}

base::FilePath GetIOSOutputDirectory() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .Append(kFeatureName)
      .Append(kTestName)
      .AppendASCII("output");
}

const std::vector<base::FilePath> GetTestFiles() {
  base::FilePath dir(GetIOSInputDirectory());
  std::string input_list_string;
  if (!base::ReadFileToString(dir.AppendASCII("autofill_test_files"),
                              &input_list_string)) {
    return {};
  }
  std::vector<base::FilePath> result;
  for (const base::StringPiece& piece :
       base::SplitStringPiece(input_list_string, "\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    result.push_back(dir.AppendASCII(piece));
  }
  return result;
}

}  // namespace

// Test fixture for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
// This is based on FormStructureBrowserTest from the Chromium Project.
// TODO(crbug.com/245246): Unify the tests.
class FormStructureBrowserTest
    : public PlatformTest,
      public testing::DataDrivenTest,
      public testing::WithParamInterface<base::FilePath> {
 public:
  FormStructureBrowserTest(const FormStructureBrowserTest&) = delete;
  FormStructureBrowserTest& operator=(const FormStructureBrowserTest&) = delete;

 protected:
  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override {}

  void SetUp() override;
  void TearDown() override;

  bool LoadHtmlWithoutSubresourcesAndInitRendererIds(const std::string& html);

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Serializes the given `forms` into a string.
  std::string FormStructuresToString(
      const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms);

  web::WebState* web_state() const { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;
  AutofillAgent* autofill_agent_;
  FormSuggestionController* suggestion_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;
  PasswordController* password_controller_;
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName),
      web_client_(std::make_unique<ChromeWebClient>()) {
  TestChromeBrowserState::Builder builder;
  builder.AddTestingFactory(
      IOSChromePasswordStoreFactory::GetInstance(),
      base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                          web::BrowserState,
                          password_manager::MockPasswordStoreInterface>));
  builder.AddTestingFactory(
      IOSUserEventServiceFactory::GetInstance(),
      base::BindRepeating(
          [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::FakeUserEventService>();
          }));
  browser_state_ = builder.Build();

  web::WebState::CreateParams params(browser_state_.get());
  web_state_ = web::WebState::Create(params);
  feature_list_.InitWithFeatures(
      // Enabled
      {// TODO(crbug.com/1098943): Remove once experiment is over.
       autofill::features::kAutofillEnableSupportForMoreStructureInNames,
       // TODO(crbug.com/1125978): Remove once launched.
       autofill::features::kAutofillEnableSupportForMoreStructureInAddresses,
       // TODO(crbug.com/1076175) Remove once launched.
       autofill::features::kAutofillUseNewSectioningMethod,
       // TODO(crbug.com/1150890) Remove once launched
       autofill::features::kAutofillEnableAugmentedPhoneCountryCode,
       // TODO(crbug.com/1157405) Remove once launched.
       autofill::features::kAutofillEnableDependentLocalityParsing,
       // TODO(crbug.com/1165780): Remove once shared labels are launched.
       autofill::features::kAutofillEnableSupportForParsingWithSharedLabels,
       // TODO(crbug.com/1150895) Remove once launched.
       autofill::features::kAutofillParsingPatternProvider,
       autofill::features::kAutofillPageLanguageDetection,
       // TODO(crbug.com/1277480): Remove once launched.
       autofill::features::kAutofillEnableNameSurenameParsing,
       // TODO(crbug.com/1190334): Remove once launched.
       autofill::features::kAutofillParseMerchantPromoCodeFields},
      // Disabled
      {});
}

void FormStructureBrowserTest::SetUp() {
  PlatformTest::SetUp();

  // Create a PasswordController instance that will handle set up for renderer
  // ids.
  UniqueIDDataTabHelper::CreateForWebState(web_state());
  password_controller_ =
      [[PasswordController alloc] initWithWebState:web_state()];

  // AddressNormalizerFactory must be initialized in a blocking allowed scoped.
  // Initialize it now as it may DCHECK if it is initialized during the test.
  AddressNormalizerFactory::GetInstance();

  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                        webState:web_state()];
  suggestion_controller_ =
      [[FormSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofill_agent_ ]];

  InfoBarManagerImpl::CreateForWebState(web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  autofill_client_.reset(new autofill::ChromeAutofillClientIOS(
      browser_state_.get(), web_state(), infobar_manager, autofill_agent_,
      /*password_generation_manager=*/nullptr));

  std::string locale("en");
  autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
      web_state(), autofill_client_.get(), /*autofill_agent=*/nil, locale,
      autofill::AutofillManager::EnableDownloadManager(false));
}

void FormStructureBrowserTest::TearDown() {
  web::test::WaitForBackgroundTasks();
  web_state_.reset();
}

bool FormStructureBrowserTest::LoadHtmlWithoutSubresourcesAndInitRendererIds(
    const std::string& html) {
  bool success = web::test::LoadHtmlWithoutSubresources(
      base::SysUTF8ToNSString(html), web_state());
  if (!success) {
    return false;
  }

  __block web::WebFrame* main_frame = nullptr;
  success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
    return main_frame != nullptr;
  });
  if (!success) {
    return false;
  }
  DCHECK(main_frame);

  uint32_t next_available_id = 1;
  autofill::FormUtilJavaScriptFeature::GetInstance()
      ->SetUpForUniqueIDsWithInitialState(main_frame, next_available_id);

  // Wait for `SetUpForUniqueIDsWithInitialState` to complete.
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return [web::test::ExecuteJavaScript(@"document[__gCrWeb.fill.ID_SYMBOL]",
                                         web_state()) intValue] ==
           static_cast<int>(next_available_id);
  });
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  ASSERT_TRUE(LoadHtmlWithoutSubresourcesAndInitRendererIds(input));
  base::ThreadPoolInstance::Get()->FlushForTesting();
  web::WebFrame* frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  BrowserAutofillManager* autofill_manager =
      AutofillDriverIOS::FromWebStateAndWebFrame(web_state(), frame)
          ->autofill_manager();
  ASSERT_NE(nullptr, autofill_manager);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool() {
        return autofill_manager->NumFormsDetected() != 0;
      }));
  *output = FormStructuresToString(autofill_manager->form_structures());
}

std::string FormStructureBrowserTest::FormStructuresToString(
    const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms) {
  std::string forms_string;
  // The forms are sorted by their global ID, which should make the order
  // deterministic.
  for (const auto& form_kv : forms) {
    const auto* form = form_kv.second.get();
    std::map<std::string, int> section_to_index;
    for (const auto& field : *form) {
      std::string name = base::UTF16ToUTF8(field->name);
      if (base::StartsWith(name, "gChrome~field~",
                           base::CompareCase::SENSITIVE)) {
        // The name has been generated by iOS JavaScript. Output an empty name
        // to have a behavior similar to other platforms.
        name = "";
      }

      std::string section = field->section;
      if (base::StartsWith(section, "gChrome~field~",
                           base::CompareCase::SENSITIVE)) {
        // The name has been generated by iOS JavaScript. Output an empty name
        // to have a behavior similar to other platforms.
        size_t first_underscore = section.find_first_of('_');
        section = section.substr(first_underscore);
      }

      // Normalize the section by replacing the unique but platform-dependent
      // integers in `field->section` with consecutive unique integers.
      // The section string is of the form "fieldname_id1_id2-suffix", where
      // id1, id2 are platform-dependent and thus need to be substituted.
      size_t last_underscore = section.find_last_of('_');
      size_t second_last_underscore =
          section.find_last_of('_', last_underscore - 1);
      size_t next_dash = section.find_first_of('-', last_underscore);
      int new_section_index = static_cast<int>(section_to_index.size() + 1);
      int section_index =
          section_to_index.insert(std::make_pair(section, new_section_index))
              .first->second;
      if (second_last_underscore != std::string::npos &&
          next_dash != std::string::npos) {
        section = base::StringPrintf(
            "%s%d%s", section.substr(0, second_last_underscore + 1).c_str(),
            section_index, section.substr(next_dash).c_str());
      }

      forms_string += field->Type().ToString();
      forms_string += " | " + name;
      forms_string += " | " + base::UTF16ToUTF8(field->label);
      forms_string += " | " + base::UTF16ToUTF8(field->value);
      forms_string += " | " + section;
      forms_string += "\n";
    }
  }
  return forms_string;
}

namespace {

// To disable a data driven test, please add the name of the test file
// (i.e., "NNN_some_site.html") as a literal to the initializer_list given
// to the failing_test_names constructor.
const auto& GetFailingTestNames() {
  static std::set<std::string> failing_test_names{
      // TODO(crbug.com/1187842): These pages contains iframes. Until filling
      // across iframes is also supported on iOS, iOS has has different
      // expectations compared to non-iOS platforms.
      "049_register_ebay.com.html",
      "148_payment_dickblick.com.html",
  };
  return failing_test_names;
}

}  // namespace

// If disabling a test, prefer to add the name names of the specific test cases
// to GetFailingTestNames(), directly above, instead of renaming the test to
// DISABLED_DataDrivenHeuristics.
TEST_P(FormStructureBrowserTest, DataDrivenHeuristics) {
  bool is_expected_to_pass =
      !base::Contains(GetFailingTestNames(), GetParam().BaseName().value());
  RunOneDataDrivenTest(GetParam(), GetIOSOutputDirectory(),
                       is_expected_to_pass);
}

INSTANTIATE_TEST_SUITE_P(AllForms,
                         FormStructureBrowserTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace autofill
