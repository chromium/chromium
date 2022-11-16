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
  void SetUpCommandLine(base::CommandLine* command_line) override;

  virtual base::Value::Dict GetPageUrlExtraParams();

  virtual bool RequiresSitePerProcess();

 private:
  // HeadlessWebContentsObserver implementation.
  void RunDevTooledTest() override;

  void OnLoadEventFired(const base::Value::Dict& params);
  void OnEvaluateResult(base::Value::Dict params);
  void OnConsoleAPICalled(const base::Value::Dict& params);

  void ProcessTestResult(const std::string& test_result);
  void FinishTest();

 protected:
  bool test_finished_ = false;
  std::string test_folder_;
  std::string script_name_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_
