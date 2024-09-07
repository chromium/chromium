// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "sandbox/policy/features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/win/sandbox_win.h"
#endif

namespace content {

namespace {

constexpr char kEnabledDomain[] = "enable-jit.net";
constexpr char kDisabledDomain[] = "disable-jit.net";

bool RendererIsJitless(RenderProcessHost* rph) {
  // It would be nice if we could also check the renderer process's actual
  // command line here, but there's no portable interface to do so.
  return rph->IsJitDisabled();
}

#if BUILDFLAG(IS_WIN)
bool RendererHasDynamicCodeMitigation(RenderProcessHost* rph) {
  // Multiple renderer processes might have started. Grab a reference to the
  // base::Process itself as well as grabbing the process ID; this ensures that
  // the process doesn't actually die during the RunLoop::Run() call below, so
  // its pid cannot be reused and confuse the test.
  base::Process proc = rph->GetProcess().Duplicate();
  base::ProcessId renderer_process_id = proc.Pid();

  base::RunLoop run_loop;
  base::Value out_args;
  sandbox::policy::SandboxWin::GetPolicyDiagnostics(
      base::BindLambdaForTesting([&run_loop, &out_args](base::Value args) {
        out_args = std::move(args);
        run_loop.Quit();
      }));
  run_loop.Run();

  const base::Value::List* process_list = out_args.GetIfList();
  CHECK(process_list);

  for (const base::Value& process_value : *process_list) {
    const base::Value::Dict* process = process_value.GetIfDict();
    CHECK(process);
    double pid = *process->FindDouble("processId");
    if (base::checked_cast<base::ProcessId>(pid) != renderer_process_id) {
      continue;
    }

    std::string mitigations = *process->FindString("desiredMitigations");
    uint64_t mask = 0;
    CHECK(base::HexStringToUInt64(mitigations, &mask));

    return !!(mask & sandbox::MITIGATION_DYNAMIC_CODE_DISABLE);
  }

  return false;
}
#endif  // IS_WIN

}  // namespace

class JitPolicyContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool IsJitDisabledForSite(BrowserContext* context, const GURL& url) override {
    // This is a bit confusing: for kEnabledDomain, JIT is enabled, so
    // IsJitDisabled() returns false. For kDisabledDomain, JIT is disabled, so
    // IsJitDisabled() returns true. Otherwise, use the default policy.
    if (url.DomainIs(kEnabledDomain)) {
      return false;
    } else if (url.DomainIs(kDisabledDomain)) {
      return true;
    } else {
      return ContentBrowserTestContentBrowserClient::IsJitDisabledForSite(
          context, url);
    }
  }

  // Always enable renderer code integrity - without this, ProhibitDynamicCode
  // never gets applied to the renderer anyway.
#if BUILDFLAG(IS_WIN)
  bool IsRendererCodeIntegrityEnabled() override { return true; }
#endif  // IS_WIN
};

// This test fixture installs a test ContentBrowserClient which enables JIT for
// kEnabledDomain and disables JIT for kDisabledDomain.
class JitPolicyBrowserTest : public ContentBrowserTest {
 public:
  JitPolicyBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    content_browser_client_ = std::make_unique<JitPolicyContentBrowserClient>();
  }

 protected:
  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  // Constructing this object causes it to register itself as a
  // ContentBrowserClient.
  std::unique_ptr<JitPolicyContentBrowserClient> content_browser_client_;
};

// This test asserts that navigating to a renderer which has JIT disabled yields
// a jitless renderer process with the DynamicCode mitigation applied.
IN_PROC_BROWSER_TEST_F(JitPolicyBrowserTest, JitDisabledImpliesJitless) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kDisabledDomain, "/title1.html")));

  RenderProcessHost* rph = contents()->GetPrimaryMainFrame()->GetProcess();

  // With JIT disabled, the renderer process should be jitless and have the
  // DynamicCode mitigation applied.
  EXPECT_TRUE(RendererIsJitless(rph));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(RendererHasDynamicCodeMitigation(rph));
#endif
}

// This test asserts that navigating to a JIT-enabled site results in a renderer
// process that is not jitless and does not have the dynamic code mitigation
// enabled.
IN_PROC_BROWSER_TEST_F(JitPolicyBrowserTest, JitEnabledImpliesNoJitless) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kEnabledDomain, "/title1.html")));

  RenderProcessHost* rph = contents()->GetPrimaryMainFrame()->GetProcess();

  // With JIT enabled, the renderer process should have JIT and not have the
  // DynamicCode mitigation applied.
  EXPECT_FALSE(RendererIsJitless(rph));
#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(RendererHasDynamicCodeMitigation(rph));
#endif
}

}  // namespace content
