// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_driven_test.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

namespace {

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
      .AppendASCII("autofill")
      .Append(kTestName)
      .AppendASCII("input");
}

base::FilePath GetIOSOutputDirectory() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("autofill")
      .Append(kTestName)
      .AppendASCII("output");
}

const std::vector<base::FilePath> GetTestFiles() {
  base::FilePath dir(GetIOSInputDirectory());
  base::FileEnumerator input_files(dir, false, base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

  base::mac::ClearAmIBundledCache();
  return files;
}

}  // namespace

// Test fixture for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
// This is based on FormStructureBrowserTest from the Chromium Project.
// TODO(crbug.com/245246): Unify the tests.
class FormStructureBrowserTest
    : public ChromeWebTest,
      public DataDrivenTest,
      public ::testing::WithParamInterface<base::FilePath> {
 protected:
  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override {}

  void SetUp() override;
  void TearDown() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Serializes the given |forms| into a string.
  std::string FormStructuresToString(
      const AutofillManager::FormStructureMap& forms);

  std::unique_ptr<autofill::ChromeAutofillClientIOS> autofill_client_;
  AutofillAgent* autofill_agent_;
  FormSuggestionController* suggestion_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(FormStructureBrowserTest);
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : ChromeWebTest(std::make_unique<ChromeWebClient>()),
      DataDrivenTest(GetTestDataDir()) {
  feature_list_.InitWithFeatures(
      // Enabled
      {},
      // Disabled
      {autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics,
       autofill::features::kAutofillEnforceMinRequiredFieldsForQuery,
       autofill::features::kAutofillEnforceMinRequiredFieldsForUpload,
       autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout});
}

void FormStructureBrowserTest::SetUp() {
  ChromeWebTest::SetUp();

  // AddressNormalizerFactory must be initialized in a blocking allowed scoped.
  // Initialize it now as it may DCHECK if it is initialized during the test.
  AddressNormalizerFactory::GetInstance();

  autofill_agent_ = [[AutofillAgent alloc]
      initWithPrefService:chrome_browser_state_->GetPrefs()
                 webState:web_state()];
  suggestion_controller_ =
      [[FormSuggestionController alloc] initWithWebState:web_state()
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
}

void FormStructureBrowserTest::TearDown() {
  ChromeWebTest::TearDown();
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  ASSERT_TRUE(LoadHtmlWithoutSubresources(input));
  base::ThreadPoolInstance::Get()->FlushForTesting();
  web::WebFrame* frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  AutofillManager* autofill_manager =
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
    const AutofillManager::FormStructureMap& forms) {
  std::map<base::TimeTicks, const FormStructure*> sorted_forms;
  for (const auto& form_kv : forms) {
    const auto* form = form_kv.second.get();
    EXPECT_TRUE(
        sorted_forms.emplace(form->form_parsed_timestamp(), form).second);
  }

  std::string forms_string;
  for (const auto& form_kv : sorted_forms) {
    const auto* form = form_kv.second;
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
const std::set<std::string>& GetFailingTestNames() {
  static std::set<std::string>* failing_test_names =
      new std::set<std::string>{};
  return *failing_test_names;
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
