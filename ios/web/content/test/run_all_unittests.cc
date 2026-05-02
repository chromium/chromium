// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"

namespace {

// UnitTestTestSuite initializes Blink, which loads CSS default stylesheets
// from a pak file via the resource bundle. Subclass ContentTestSuiteBase to
// install the resource bundle in `Initialize()` before Blink is brought up.
class IOSWebContentTestSuite : public content::ContentTestSuiteBase {
 public:
  IOSWebContentTestSuite(int argc, char** argv)
      : content::ContentTestSuiteBase(argc, argv) {}

 protected:
  void Initialize() override {
    InitializeResourceBundle();
    content::ContentTestSuiteBase::Initialize();
  }
};

}  // namespace

int main(int argc, char** argv) {
  // content::UnitTestTestSuite installs the standard content unit-test
  // environment. In particular, it calls
  // ForceCreateNetworkServiceDirectlyForTesting() so the in-process network
  // service runs on the IO thread managed by BrowserTaskEnvironment instead
  // of a dedicated thread, which would otherwise race the global
  // ThreadPoolInstance shutdown when a test fixture is torn down. It also sets
  // up TestContentClient and TestContentBrowserClient per test, so individual
  // fixtures should not install their own.
  content::UnitTestTestSuite test_suite(
      new IOSWebContentTestSuite(argc, argv),
      base::BindRepeating(content::UnitTestTestSuite::CreateTestContentClients),
      /*child_mojo_config=*/std::nullopt);
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
