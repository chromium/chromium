// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Test class to verify that the RendererAppContainer feature works correctly.
class RendererAppContainerFeatureBrowserTest : public ContentBrowserTest {
 public:
  RendererAppContainerFeatureBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        sandbox::policy::features::kRendererAppContainer, true);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RendererAppContainerFeatureBrowserTest, Navigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
}

// Test class to verify the behavior underlying chrome://sandbox for renderers.
class SandboxDiagnosticsBrowserTest : public ContentBrowserTest {
 public:
  SandboxDiagnosticsBrowserTest() = default;

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

IN_PROC_BROWSER_TEST_F(SandboxDiagnosticsBrowserTest, Navigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));

  // Duplicate the base::Process to keep a valid Windows handle to to the
  // process open, this ensures that even if the RPH gets destroyed during the
  // runloop below, the handle to the process remains valid, and the pid is
  // never reused by Windows.
  const auto renderer_process =
      contents()->GetPrimaryMainFrame()->GetProcess()->GetProcess().Duplicate();

  base::RunLoop run_loop;
  base::Value out_args;
  sandbox::policy::SandboxWin::GetPolicyDiagnostics(
      base::BindLambdaForTesting([&run_loop, &out_args](base::Value args) {
        out_args = std::move(args);
        run_loop.Quit();
      }));
  run_loop.Run();

  const base::Value::List* process_list = out_args.GetIfList();
  ASSERT_TRUE(process_list);
  bool found_renderer = false;
  bool found_device_api = false;
  bool found_ksec_dd = false;

  for (const base::Value& process_value : *process_list) {
    const base::Value::Dict* process = process_value.GetIfDict();
    ASSERT_TRUE(process);
    std::optional<double> pid = process->FindDouble("processId");
    ASSERT_TRUE(pid.has_value());
    if (base::checked_cast<base::ProcessId>(pid.value()) !=
        renderer_process.Pid()) {
      continue;
    }
    found_renderer = true;
    auto* rules = process->FindList("handlesToClose");
    ASSERT_TRUE(rules);

    // Validate that there are a couple of listed handle names to be sure there
    // is something in the rule.
    for (const base::Value& opcode : *rules) {
      auto* value = opcode.GetIfString();
      ASSERT_TRUE(value);
      if (value->find("DeviceApi") != value->npos) {
        found_device_api = true;
      }
      if (value->find("KsecDD") != value->npos) {
        found_ksec_dd = true;
      }
    }
  }

  EXPECT_TRUE(found_renderer);
  EXPECT_TRUE(found_ksec_dd);
  EXPECT_TRUE(found_device_api);
}

}  // namespace content
