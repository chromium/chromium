// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_
#define HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/values.h"
#include "components/headless/test/test_meta_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/browser_test.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_devtooled_browsertest.h"

namespace headless {

class HeadlessProtocolBrowserTest : public HeadlessDevTooledBrowserTest {
 public:
  HeadlessProtocolBrowserTest();
  ~HeadlessProtocolBrowserTest() override;

 protected:
  // Implement this to provide the relative script name;
  virtual std::string GetScriptName() = 0;

  // Implement this for tests that need to pass extra parameters to
  // JavaScript test body.
  virtual base::Value::Dict GetPageUrlExtraParams();

  // Returns relative test data directory.
  base::FilePath GetTestDataDir();

  // Returns absolute script file path.
  base::FilePath GetScriptPath();

  // Returns absolute expectations file path.
  base::FilePath GetTestExpectationFilePath();

  bool IsSharedTestScript();

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  void StartEmbeddedTestServer();

  // HeadlessDevTooledBrowserTest:
  void RunDevTooledTest() override;

  void OnceSetUp(base::Value::Dict params);
  void OnLoadEventFired(const base::Value::Dict& params);
  void OnEvaluateResult(base::Value::Dict params);

  void ProcessTestResult(const std::string& test_result);
  void FinishTest();

 protected:
  void LoadTestMetaInfo();

  TestMetaInfo test_meta_info_;
  bool test_finished_ = false;
};

#define HEADLESS_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME) \
  HEADLESS_PROTOCOL_TEST_F(HeadlessProtocolBrowserTest, TEST_NAME, SCRIPT_NAME)

#define HEADLESS_PROTOCOL_TEST_F(FIXTURE_NAME, TEST_NAME, SCRIPT_NAME) \
  class FIXTURE_NAME##_##TEST_NAME : public FIXTURE_NAME {             \
   public:                                                             \
    std::string GetScriptName() override {                             \
      return SCRIPT_NAME;                                              \
    }                                                                  \
  };                                                                   \
                                                                       \
  IN_PROC_BROWSER_TEST_F(FIXTURE_NAME##_##TEST_NAME, TEST_NAME) {      \
    RunTest();                                                         \
  }

#define HEADLESS_PROTOCOL_TEST_P(FIXTURE_NAME, TEST_NAME, SCRIPT_NAME) \
  class FIXTURE_NAME##_##TEST_NAME : public FIXTURE_NAME {             \
   public:                                                             \
    std::string GetScriptName() override {                             \
      return SCRIPT_NAME;                                              \
    }                                                                  \
  };                                                                   \
                                                                       \
  IN_PROC_BROWSER_TEST_P(FIXTURE_NAME##_##TEST_NAME, TEST_NAME) {      \
    RunTest();                                                         \
  }

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_
