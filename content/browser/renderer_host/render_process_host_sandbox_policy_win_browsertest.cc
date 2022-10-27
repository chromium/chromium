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

// Test class to verify the behavior of the pipe interceptions for renderers.
class PipeLockdownFeatureBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface</* Pipe Lockdown Enabled */ bool> {
 public:
  PipeLockdownFeatureBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        sandbox::policy::features::kChromePipeLockdown, PipeLockdownEnabled());
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  bool PipeLockdownEnabled() const { return GetParam(); }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PipeLockdownFeatureBrowserTest, Navigate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));

  // Multiple renderer processes might have started. It is safe to hold this Pid
  // here because no renderers can start or stop while on the UI thread.
  base::ProcessId renderer_process_id =
      contents()->GetPrimaryMainFrame()->GetProcess()->GetProcess().Pid();

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
  bool found_chrome_pipe_create_pipe_rule = false;
  bool found_chrome_pipe_open_rule = false;
  bool found_chrome_sync_pipe_create_pipe_rule = false;
  bool found_chrome_sync_pipe_open_rule = false;
  for (const base::Value& process_value : *process_list) {
    const base::Value::Dict* process = process_value.GetIfDict();
    ASSERT_TRUE(process);
    absl::optional<double> pid = process->FindDouble("processId");
    ASSERT_TRUE(pid.has_value());
    if (base::checked_cast<base::ProcessId>(pid.value()) != renderer_process_id)
      continue;
    found_renderer = true;
    auto* rules = process->FindDict("policyRules");
    ASSERT_TRUE(rules);

    const base::Value::List* open_file_opcodes = rules->FindList("NtOpenFile");
    if (open_file_opcodes) {
      for (const base::Value& opcode : *open_file_opcodes) {
        auto* value = opcode.GetIfString();
        ASSERT_TRUE(value);
        if (value->find("chrome.'") != value->npos)
          found_chrome_pipe_open_rule = true;
        if (value->find("chrome.sync.'") != value->npos)
          found_chrome_sync_pipe_open_rule = true;
      }
    }

    const base::Value::List* create_named_pipe_opcodes =
        rules->FindList("CreateNamedPipeW");
    if (create_named_pipe_opcodes) {
      for (const base::Value& opcode : *create_named_pipe_opcodes) {
        auto* value = opcode.GetIfString();
        ASSERT_TRUE(value);
        if (value->find("chrome.'") != value->npos)
          found_chrome_pipe_create_pipe_rule = true;
        if (value->find("chrome.sync.'") != value->npos)
          found_chrome_sync_pipe_create_pipe_rule = true;
      }
    }
  }

  EXPECT_TRUE(found_renderer);

  // There should never be an NtOpenFile rule for chrome.sync.*.
  EXPECT_FALSE(found_chrome_sync_pipe_open_rule);

  // There should never be a way to Create pipes for chrome.*.
  EXPECT_FALSE(found_chrome_pipe_create_pipe_rule);

  if (PipeLockdownEnabled()) {
    // With pipe lockdown enabled, no pipe rules should exist for renderers.
    EXPECT_FALSE(found_chrome_sync_pipe_create_pipe_rule);
    EXPECT_FALSE(found_chrome_pipe_open_rule);
  } else {
    // Old behavior allowed chrome.sync.* to be created (for base::SyncSocket).
    EXPECT_TRUE(found_chrome_sync_pipe_create_pipe_rule);
    // Old behavior allowed chrome.* to be opened (for legacy IPC).
    EXPECT_TRUE(found_chrome_pipe_open_rule);
  }
}

INSTANTIATE_TEST_SUITE_P(Enabled,
                         PipeLockdownFeatureBrowserTest,
                         /* Pipe Lockdown Enabled */ ::testing::Values(true));

INSTANTIATE_TEST_SUITE_P(Disabled,
                         PipeLockdownFeatureBrowserTest,
                         /* Pipe Lockdown Enabled */ ::testing::Values(false));

}  // namespace content
