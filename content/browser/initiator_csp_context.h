// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INITIATOR_CSP_CONTEXT_H_
#define CONTENT_BROWSER_INITIATOR_CSP_CONTEXT_H_

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/navigation_params.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

namespace content {

// This is a CSP context that represents the document that initiated a
// navigation. The reason we can't just use the initiating RenderFrameHost is
// because it might already be destroyed when the CSP are checked in the
// navigating RenderFrameHost. This uses the policies
// passed through the CommonNavigationParams. The
// relevant CSP directives of the navigation initiator are currently
// `navigate-to` and `form-action` (in the case of form submissions).
class InitiatorCSPContext : public network::CSPContext {
 public:
  InitiatorCSPContext(
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies,
      network::mojom::CSPSourcePtr self_source,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator);
  ~InitiatorCSPContext() override;

  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) override;
  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override;
  void SetReportingRenderFrameHost(RenderFrameHostImpl* rfh);
  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override;

 private:
  RenderFrameHostImpl* reporting_render_frame_host_impl_;
  mojo::Remote<blink::mojom::NavigationInitiator> initiator;

  DISALLOW_COPY_AND_ASSIGN(InitiatorCSPContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INITIATOR_CSP_CONTEXT_H_
