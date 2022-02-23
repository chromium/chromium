// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_unit_test_suite.h"

#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/test/test_simple_task_runner.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ios/chrome/browser/browser_state/browser_state_keyed_service_factories.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/test/testing_application_context.h"
#include "ios/components/webui/web_ui_url_constants.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

    chrome_browser_provider_ = ios::CreateChromeBrowserProvider();
    ios::ChromeBrowserProvider* previous_provider =
        ios::SetChromeBrowserProvider(chrome_browser_provider_.get());
    DCHECK(!previous_provider);

    DCHECK(!GetApplicationContext());
    application_context_.reset(new TestingApplicationContext);
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    DCHECK_EQ(GetApplicationContext(), application_context_.get());
    application_context_.reset();

    ios::ChromeBrowserProvider* previous_provider =
        ios::SetChromeBrowserProvider(nullptr);
    DCHECK_EQ(previous_provider, chrome_browser_provider_.get());
    chrome_browser_provider_.reset();
  }

 private:
  std::unique_ptr<ios::ChromeBrowserProvider> chrome_browser_provider_;
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

  // Ensure that all BrowserStateKeyedServiceFactories are built before any
  // test is run so that the dependencies are correctly resolved.
  EnsureBrowserStateKeyedServiceFactoriesBuilt();

  // Register a SingleThreadTaskRunner for base::RecordAction as overridding
  // it in individual tests is unsafe (as there is no way to unregister).
  base::SetRecordActionTaskRunner(action_task_runner_);

  ios::RegisterPathProvider();
  ui::RegisterPathProvider();
  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(nullptr, 0);
}
