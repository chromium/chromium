// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "content/public/test/browser_test.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "headless/test/headless_devtooled_browsertest.h"
#include "testing/gtest/include/gtest/gtest.h"

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {

class HeadlessClientBrowserTest : public HeadlessDevTooledBrowserTest {
 public:
  HeadlessClientBrowserTest() = default;

 private:
  void RunDevTooledTest() override {
    browser_devtools_client_.SendCommand(
        "Target.createTarget", Param("url", "about:blank"),
        base::BindOnce(&HeadlessClientBrowserTest::AttachToTarget,
                       base::Unretained(this)));
  }

  void AttachToTarget(base::Value::Dict result) {
    base::Value::Dict params;
    params.Set("targetId", DictString(result, "result.targetId"));
    params.Set("flatten", true);
    browser_devtools_client_.SendCommand(
        "Target.attachToTarget", std::move(params),
        base::BindOnce(&HeadlessClientBrowserTest::CreateSession,
                       base::Unretained(this)));
  }

  void CreateSession(base::Value::Dict result) {
    session_client_ = browser_devtools_client_.CreateSession(
        DictString(result, "result.sessionId"));

    session_client_->SendCommand(
        "Runtime.evaluate", Param("expression", "window.location.href"),
        base::BindOnce(&HeadlessClientBrowserTest::FinishTest,
                       base::Unretained(this)));
  }

  void FinishTest(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasValue("result.result.value", "about:blank"));
    session_client_.reset();
    FinishAsynchronousTest();
  }

 private:
  std::unique_ptr<SimpleDevToolsProtocolClient> session_client_;
};

IN_PROC_BROWSER_TEST_F(HeadlessClientBrowserTest, FlatProtocolAccess) {
  RunTest();
}

}  // namespace headless
