// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/target.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

class HeadlessClientBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                                  public target::ExperimentalObserver {
 public:
  HeadlessClientBrowserTest() = default;

 private:
  // HeadlessWebContentsObserver implementation.
  void RunDevTooledTest() override {
    browser_devtools_client_->GetTarget()->GetExperimental()->AddObserver(this);
    browser_devtools_client_->GetTarget()->CreateTarget(
        target::CreateTargetParams::Builder().SetUrl("about:blank").Build(),
        base::BindOnce(&HeadlessClientBrowserTest::AttachToTarget,
                       base::Unretained(this)));
  }

  void AttachToTarget(std::unique_ptr<target::CreateTargetResult> result) {
    browser_devtools_client_->GetTarget()->AttachToTarget(
        target::AttachToTargetParams::Builder()
            .SetTargetId(result->GetTargetId())
            .SetFlatten(true)
            .Build(),
        base::BindOnce(&HeadlessClientBrowserTest::CreateSession,
                       base::Unretained(this)));
  }

  void CreateSession(std::unique_ptr<target::AttachToTargetResult> result) {
    session_client_ =
        browser_devtools_client_->CreateSession(result->GetSessionId());
    session_client_->GetRuntime()->Evaluate(
        "window.location.href",
        base::BindOnce(&HeadlessClientBrowserTest::FinishTest,
                       base::Unretained(this)));
  }

  void FinishTest(std::unique_ptr<runtime::EvaluateResult> result) {
    const base::Value* value = result->GetResult()->GetValue();
    std::string str;
    EXPECT_TRUE(value->GetAsString(&str));
    EXPECT_EQ("about:blank", str);
    session_client_.reset();
    FinishAsynchronousTest();
  }

 private:
  std::unique_ptr<HeadlessDevToolsClient> session_client_;
};

IN_PROC_BROWSER_TEST_F(HeadlessClientBrowserTest, FlatProtocolAccess) {
  RunTest();
}

}  // namespace headless
