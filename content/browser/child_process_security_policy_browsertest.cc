// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
bool AndroidWillCreateSpareRendererWithTimeout() {
  return base::FeatureList::IsEnabled(
      features::kAndroidWarmUpSpareRendererWithTimeout);
}
}  // namespace

class ChildProcessSecurityPolicyInProcessBrowserTest
    : public ContentBrowserTest {
 public:
  void SetUp() override {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->security_state_.size());
    }
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->security_state_.size());
    }
    ContentBrowserTest::TearDown();
  }
};

#if !defined(NDEBUG) && BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ChildProcessSecurityPolicyInProcessBrowserTest,
                       DISABLED_NoLeak) {
#else
IN_PROC_BROWSER_TEST_F(ChildProcessSecurityPolicyInProcessBrowserTest, NoLeak) {
#endif
  GURL url = GetTestUrl("", "simple_page.html");
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const bool kWillCreateSpareRenderer =
      RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes() ||
      AndroidWillCreateSpareRendererWithTimeout();

  EXPECT_TRUE(NavigateToURL(shell(), url));
  {
    base::AutoLock lock(policy->lock_);
    EXPECT_EQ(kWillCreateSpareRenderer ? 2u : 1u,
              policy->security_state_.size());
  }

  WebContents* web_contents = shell()->web_contents();
  content::RenderProcessHostWatcher exit_observer(
      web_contents->GetPrimaryMainFrame()->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
      RESULT_CODE_KILLED);
  exit_observer.Wait();

  web_contents->GetController().Reload(ReloadType::NORMAL, true);
  {
    base::AutoLock lock(policy->lock_);
    EXPECT_EQ(kWillCreateSpareRenderer ? 2u : 1u,
              policy->security_state_.size());
  }
}

}  // namespace content
