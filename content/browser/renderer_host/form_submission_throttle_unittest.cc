// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"

namespace content {

class FormSubmissionTest : public RenderViewHostImplTestHarness {
 public:
  void PreventFormSubmission() {
    auto source_none = network::mojom::CSPSourceList::New();
    auto policy = network::mojom::ContentSecurityPolicy::New();
    policy->header = network::mojom::ContentSecurityPolicyHeader::New();
    policy->directives[network::mojom::CSPDirectiveName::FormAction] =
        std::move(source_none);
    main_test_rfh()->AddContentSecurityPolicy(std::move(policy));
  }
};

// Tests that form submissions are allowed by default when there is no CSP.
TEST_F(FormSubmissionTest, ContentSecurityPolicyFormActionNoCSP) {
  const GURL kUrl("https://chromium.org");
  const GURL kFormUrl("https://foo.com");
  const GURL kRedirectUrl("https://bar.com");

  // Load a page.
  NavigateAndCommit(kUrl);

  // Try to submit a form.
  auto form_submission =
      NavigationSimulatorImpl::CreateRendererInitiated(kFormUrl, main_rfh());
  form_submission->SetIsFormSubmission(true);
  form_submission->set_should_check_main_world_csp(
      network::mojom::CSPDisposition::CHECK);
  form_submission->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            form_submission->GetLastThrottleCheckResult());
  form_submission->Redirect(kRedirectUrl);
  EXPECT_EQ(NavigationThrottle::PROCEED,
            form_submission->GetLastThrottleCheckResult());
}

// Tests that no form submission is allowed when the calling RenderFrameHost's
// CSP is "form-action 'none'".
TEST_F(FormSubmissionTest, ContentSecurityPolicyFormActionNone) {
  const GURL kUrl("https://chromium.org");
  const GURL kFormUrl("https://foo.com");
  const GURL kRedirectUrl("https://bar.com");

  // Load a page.
  NavigateAndCommit(kUrl);
  PreventFormSubmission();

  // Try to submit a form.
  auto form_submission =
      NavigationSimulatorImpl::CreateRendererInitiated(kFormUrl, main_rfh());
  form_submission->SetIsFormSubmission(true);
  form_submission->set_should_check_main_world_csp(
      network::mojom::CSPDisposition::CHECK);

  // Browser side checks have been disabled on the initial load. Only the
  // renderer side checks occurs. Related issue: https://crbug.com/798698.
  form_submission->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            form_submission->GetLastThrottleCheckResult());

  form_submission->Redirect(kRedirectUrl);
  EXPECT_EQ(NavigationThrottle::CANCEL,
            form_submission->GetLastThrottleCheckResult());
}

// Tests that the navigation is allowed because "should_by_pass_main_world_csp"
// is true, even if it is a form submission and the policy is
// "form-action 'none'".
TEST_F(FormSubmissionTest, ContentSecurityPolicyFormActionBypassCSP) {
  const GURL kUrl("https://chromium.org");
  const GURL kFormUrl("https://foo.com");
  const GURL kRedirectUrl("https://bar.com");

  // Load a page.
  NavigateAndCommit(kUrl);
  PreventFormSubmission();

  // Try to submit a form.
  auto form_submission =
      NavigationSimulatorImpl::CreateRendererInitiated(kFormUrl, main_rfh());
  form_submission->SetIsFormSubmission(true);
  form_submission->set_should_check_main_world_csp(
      network::mojom::CSPDisposition::DO_NOT_CHECK);
  form_submission->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            form_submission->GetLastThrottleCheckResult());

  form_submission->Redirect(kRedirectUrl);
  EXPECT_EQ(NavigationThrottle::PROCEED,
            form_submission->GetLastThrottleCheckResult());
}

}  // namespace content
