// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ios_chrome_unit_test_suite.h"

#import "base/metrics/user_metrics.h"
#import "base/path_service.h"
#import "base/test/test_simple_task_runner.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "ios/chrome/browser/profile/model/keyed_service_factories.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/web/public/web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/ui_base_paths.h"
#import "url/url_util.h"

namespace {

class IOSChromeUnitTestSuiteInitializer
    : public testing::EmptyTestEventListener {
 public:
  IOSChromeUnitTestSuiteInitializer() {}

  IOSChromeUnitTestSuiteInitializer(const IOSChromeUnitTestSuiteInitializer&) =
      delete;
  IOSChromeUnitTestSuiteInitializer& operator=(
      const IOSChromeUnitTestSuiteInitializer&) = delete;

  ~IOSChromeUnitTestSuiteInitializer() override {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    ios::provider::Initialize();

    DCHECK(!GetApplicationContext());
    application_context_.reset(new TestingApplicationContext);
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    DCHECK_EQ(GetApplicationContext(), application_context_.get());
    application_context_.reset();

    breadcrumbs::BreadcrumbManager::GetInstance().ResetForTesting();
  }

 private:
  std::unique_ptr<ApplicationContext> application_context_;
};

}  // namespace

IOSChromeUnitTestSuite::IOSChromeUnitTestSuite(int argc, char** argv)
    : web::WebTestSuite(argc, argv),
      action_task_runner_(new base::TestSimpleTaskRunner) {}

IOSChromeUnitTestSuite::~IOSChromeUnitTestSuite() {}

void IOSChromeUnitTestSuite::Initialize() {
  url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITH_HOST);

  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new IOSChromeUnitTestSuiteInitializer);

  // Call the superclass Initialize() method after adding the listener.
  web::WebTestSuite::Initialize();

  // Ensure that all KeyedServiceFactories are built before any test is run so
  // that the dependencies are correctly resolved.
  EnsureProfileKeyedServiceFactoriesBuilt();

  // Register a SingleThreadTaskRunner for base::RecordAction as overridding
  // it in individual tests is unsafe (as there is no way to unregister).
  base::SetRecordActionTaskRunner(action_task_runner_);

  ios::RegisterPathProvider();
  ui::RegisterPathProvider();
  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(nullptr, 0);
}
