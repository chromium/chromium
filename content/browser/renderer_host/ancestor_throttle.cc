// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ancestor_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

namespace {
const char kXFrameOptionsSameOriginHistogram[] = "Security.XFrameOptions";

// This enum is used for UMA metrics. Keep these enums up to date with
// tools/metrics/histograms/histograms.xml.
enum XFrameOptionsHistogram {
  // A frame is loaded without any X-Frame-Options header.
  NONE = 0,

  // X-Frame-Options: DENY.
  DENY = 1,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds and every ancestor
  // has the same origin.
  SAMEORIGIN = 2,

  // X-Frame-Options: SAMEORIGIN. The navigation is blocked because the
  // top-frame doesn't have the same origin.
  SAMEORIGIN_BLOCKED = 3,

  // X-Frame-Options: SAMEORIGIN. The navigation proceeds despite the fact that
  // there is an ancestor that doesn't have the same origin.
  SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN = 4,

  // X-Frame-Options: ALLOWALL.
  ALLOWALL = 5,

  // Invalid 'X-Frame-Options' directive encountered.
  INVALID = 6,

  // The frame sets multiple 'X-Frame-Options' header with conflicting values.
  CONFLICT = 7,

  // The 'frame-ancestors' CSP directive should take effect instead.
  BYPASS = 8,

  // Navigation would have been blocked if we applied 'X-Frame-Options' to
  // redirects.
  //
  // TODO(mkwst): Rename this when we make a decision around
  // https://crbug.com/835465.
  REDIRECT_WOULD_BE_BLOCKED = 9,

  XFRAMEOPTIONS_HISTOGRAM_MAX = REDIRECT_WOULD_BE_BLOCKED
};

void RecordXFrameOptionsUsage(XFrameOptionsHistogram usage) {
  UMA_HISTOGRAM_ENUMERATION(
      kXFrameOptionsSameOriginHistogram, usage,
      XFrameOptionsHistogram::XFRAMEOPTIONS_HISTOGRAM_MAX);
}

bool HeadersContainFrameAncestorsCSP(
    const network::mojom::ParsedHeadersPtr& headers) {
  return base::ranges::any_of(
      headers->content_security_policy, [](const auto& csp) {
        return csp->header->type ==
                   network::mojom::ContentSecurityPolicyType::kEnforce &&
               csp->directives.count(
                   network::mojom::CSPDirectiveName::FrameAncestors);
      });
}

class FrameAncestorCSPContext : public network::CSPContext {
 public:
  FrameAncestorCSPContext(
      RenderFrameHostImpl* navigated_frame,
      const std::vector<network::mojom::ContentSecurityPolicyPtr>& policies)
      : navigated_frame_(navigated_frame) {
    // TODO(arthursonzogni): Refactor CSPContext to its original state, it
    // shouldn't own any ContentSecurityPolicies on its own. This should be
    // defined by the implementation instead. Copies could be avoided here.
    for (auto& policy : policies)
      AddContentSecurityPolicy(mojo::Clone(policy));
  }

 private:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) override {
    return navigated_frame_->ReportContentSecurityPolicyViolation(
        std::move(violation_params));
  }

  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override {
    return navigated_frame_->SchemeShouldBypassCSP(scheme);
  }

  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const override {
    return navigated_frame_->SanitizeDataForUseInCspViolation(
        is_redirect, directive, blocked_url, source_location);
  }

  RenderFrameHostImpl* navigated_frame_;
};

// Returns the parent, including outer delegates in the case of portals.
RenderFrameHostImpl* ParentOrOuterDelegate(RenderFrameHostImpl* frame) {
  return frame->InsidePortal() ? frame->ParentOrOuterDelegateFrame()
                               : frame->GetParent();
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle> AncestorThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return base::WrapUnique(new AncestorThrottle(handle));
}

AncestorThrottle::~AncestorThrottle() {}

NavigationThrottle::ThrottleCheckResult AncestorThrottle::WillStartRequest() {
  if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE))
    return NavigationThrottle::PROCEED;

  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  if (request->IsInMainFrame())
    return NavigationThrottle::PROCEED;

  // TODO(antoniosartori): Probably we should have taken a snapshot of the 'csp'
  // attribute at the beginning of the navigation and not now, since the
  // beforeunload handlers might have modified it in the meantime.
  std::vector<network::mojom::ContentSecurityPolicyPtr> frame_csp;
  frame_csp.emplace_back(
      request->frame_tree_node()->csp_attribute()
          ? request->frame_tree_node()->csp_attribute()->Clone()
          : nullptr);
  const network::mojom::ContentSecurityPolicy* parent_required_csp =
      request->frame_tree_node()->parent()->required_csp();

  std::string error_message;
  if (!network::IsValidRequiredCSPAttr(
          frame_csp, parent_required_csp,
          url::Origin::Create(navigation_handle()->GetURL()), error_message)) {
    if (frame_csp[0]) {
      navigation_handle()->GetParentFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          base::StringPrintf("The frame 'csp' attribute ('%s') is invalid and "
                             "will be discarded: %s",
                             frame_csp[0]->header->header_value.c_str(),
                             error_message.c_str()));
    }
    if (parent_required_csp)
      request->SetRequiredCSP(parent_required_csp->Clone());
    // TODO(antoniosartori): Consider instead blocking the navigation here,
    // since this seems to be insecure
    // (cf. https://github.com/w3c/webappsec-cspee/pull/11).
  } else {
    // If |frame_csp| is valid then it is not null.
    request->SetRequiredCSP(std::move(frame_csp[0]));
  }

  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillRedirectRequest() {
  // During a redirect, we don't know which RenderFrameHost we'll end up in,
  // so we can't log reliably to the console. We should be able to work around
  // this iff we decide to ship the redirect-blocking behavior, but for now
  // we'll just skip the console-logging bits to collect metrics.
  NavigationThrottle::ThrottleCheckResult result = ProcessResponseImpl(
      LoggingDisposition::DO_NOT_LOG_TO_CONSOLE, false /* is_response_check */);

  if (result.action() == NavigationThrottle::BLOCK_RESPONSE)
    RecordXFrameOptionsUsage(XFrameOptionsHistogram::REDIRECT_WOULD_BE_BLOCKED);

  // TODO(mkwst): We need to decide whether we'll be able to get away with
  // tightening the XFO check to include redirect responses once we have a
  // feel for the REDIRECT_WOULD_BE_BLOCKED numbers we're collecting above.
  // Until then, we'll allow the response to proceed: https://crbug.com/835465.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillProcessResponse() {
  return ProcessResponseImpl(LoggingDisposition::LOG_TO_CONSOLE,
                             true /* is_response_check */);
}

NavigationThrottle::ThrottleCheckResult AncestorThrottle::ProcessResponseImpl(
    LoggingDisposition logging,
    bool is_response_check) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());

  bool is_portal =
      request->frame_tree_node()->current_frame_host()->InsidePortal();
  if (request->IsInMainFrame() && !is_portal) {
    // Allow main frame navigations.
    return NavigationThrottle::PROCEED;
  }

  // 204/205 responses and downloads are not sent to the renderer and don't need
  // to be checked.
  if (is_response_check && !request->response_should_be_rendered()) {
    return NavigationThrottle::PROCEED;
  }

  const std::vector<network::mojom::ContentSecurityPolicyPtr>&
      content_security_policies =
          request->response()->parsed_headers->content_security_policy;

  // CSP: frame-ancestors is checked only for the final response.
  if (is_response_check &&
      EvaluateFrameAncestors(content_security_policies) == CheckResult::BLOCK) {
    return NavigationThrottle::BLOCK_RESPONSE;
  }

  if (EvaluateXFrameOptions(logging) == CheckResult::BLOCK)
    return NavigationThrottle::BLOCK_RESPONSE;

  // CSPEE is checked only for the final response.
  if (is_response_check &&
      EvaluateCSPEmbeddedEnforcement() == CheckResult::BLOCK) {
    return NavigationThrottle::BLOCK_RESPONSE;
  }

  return NavigationThrottle::PROCEED;
}

const char* AncestorThrottle::GetNameForLogging() {
  return "AncestorThrottle";
}

AncestorThrottle::AncestorThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

void AncestorThrottle::ParseXFrameOptionsError(const std::string& value,
                                               HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::CONFLICT ||
         disposition == HeaderDisposition::INVALID);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message;
  if (disposition == HeaderDisposition::CONFLICT) {
    message = base::StringPrintf(
        "Refused to display '%s' in a frame because it set multiple "
        "'X-Frame-Options' headers with conflicting values "
        "('%s'). Falling back to 'deny'.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  } else {
    message = base::StringPrintf(
        "Invalid 'X-Frame-Options' header encountered when loading '%s': "
        "'%s' is not a recognized directive. The header will be ignored.",
        navigation_handle()->GetURL().spec().c_str(), value.c_str());
  }

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  auto* frame = static_cast<RenderFrameHostImpl*>(
      navigation_handle()->GetRenderFrameHost());
  ParentOrOuterDelegate(frame)->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

void AncestorThrottle::ConsoleErrorXFrameOptions(
    HeaderDisposition disposition) {
  DCHECK(disposition == HeaderDisposition::DENY ||
         disposition == HeaderDisposition::SAMEORIGIN);
  if (!navigation_handle()->GetRenderFrameHost())
    return;  // Some responses won't have a RFH (i.e. 204/205s or downloads).

  std::string message = base::StringPrintf(
      "Refused to display '%s' in a frame because it set 'X-Frame-Options' "
      "to '%s'.",
      navigation_handle()->GetURL().spec().c_str(),
      disposition == HeaderDisposition::DENY ? "deny" : "sameorigin");

  // Log a console error in the parent of the current RenderFrameHost (as
  // the current RenderFrameHost itself doesn't yet have a document).
  auto* frame = static_cast<RenderFrameHostImpl*>(
      navigation_handle()->GetRenderFrameHost());
  ParentOrOuterDelegate(frame)->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

AncestorThrottle::CheckResult AncestorThrottle::EvaluateXFrameOptions(
    LoggingDisposition logging) {
  std::string header_value;
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  HeaderDisposition disposition =
      ParseXFrameOptionsHeader(request->GetResponseHeaders(), &header_value);

  // If 'X-Frame-Options' would potentially block the response, check whether
  // the 'frame-ancestors' CSP directive should take effect instead. See
  // https://www.w3.org/TR/CSP/#frame-ancestors-and-frame-options
  if (disposition != HeaderDisposition::NONE &&
      disposition != HeaderDisposition::ALLOWALL &&
      HeadersContainFrameAncestorsCSP(request->response()->parsed_headers)) {
    RecordXFrameOptionsUsage(XFrameOptionsHistogram::BYPASS);
    return CheckResult::PROCEED;
  }

  switch (disposition) {
    case HeaderDisposition::CONFLICT:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseXFrameOptionsError(header_value, disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::CONFLICT);
      return CheckResult::BLOCK;

    case HeaderDisposition::INVALID:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseXFrameOptionsError(header_value, disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::INVALID);
      // TODO(mkwst): Consider failing here.
      return CheckResult::PROCEED;

    case HeaderDisposition::DENY:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ConsoleErrorXFrameOptions(disposition);
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::DENY);
      return CheckResult::BLOCK;

    case HeaderDisposition::SAMEORIGIN: {
      // Block the request when any ancestor is not same-origin.
      RenderFrameHostImpl* parent = ParentOrOuterDelegate(
          request->frame_tree_node()->current_frame_host());
      url::Origin current_origin =
          url::Origin::Create(navigation_handle()->GetURL());
      while (parent) {
        if (!parent->GetLastCommittedOrigin().IsSameOriginWith(
                current_origin)) {
          RecordXFrameOptionsUsage(XFrameOptionsHistogram::SAMEORIGIN_BLOCKED);
          if (logging == LoggingDisposition::LOG_TO_CONSOLE)
            ConsoleErrorXFrameOptions(disposition);

          // TODO(mkwst): Stop recording this metric once we convince other
          // vendors to follow our lead with XFO: SAMEORIGIN processing.
          //
          // https://crbug.com/250309
          if (parent->GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
                  current_origin)) {
            RecordXFrameOptionsUsage(
                XFrameOptionsHistogram::SAMEORIGIN_WITH_BAD_ANCESTOR_CHAIN);
          }

          return CheckResult::BLOCK;
        }
        parent = ParentOrOuterDelegate(parent);
      }
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::SAMEORIGIN);
      return CheckResult::PROCEED;
    }

    case HeaderDisposition::NONE:
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::NONE);
      return CheckResult::PROCEED;
    case HeaderDisposition::ALLOWALL:
      RecordXFrameOptionsUsage(XFrameOptionsHistogram::ALLOWALL);
      return CheckResult::PROCEED;
  }
}

AncestorThrottle::CheckResult AncestorThrottle::EvaluateFrameAncestors(
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        content_security_policy) {
  // TODO(lfg): If the initiating document is known and correspond to the
  // navigating frame's current document, consider using:
  // navigation_request().common_params().source_location here instead.
  auto empty_source_location = network::mojom::SourceLocation::New();

  // CSP frame-ancestors are checked against the URL of every parent and are
  // reported to the navigating frame.
  FrameAncestorCSPContext csp_context(
      NavigationRequest::From(navigation_handle())->GetRenderFrameHost(),
      content_security_policy);
  csp_context.SetSelf(url::Origin::Create(navigation_handle()->GetURL()));

  // Check CSP frame-ancestors against every parent.
  // We enforce frame-ancestors in the outer delegate for portals, but not
  // for other uses of inner/outer WebContents (GuestViews).
  RenderFrameHostImpl* parent =
      ParentOrOuterDelegate(static_cast<RenderFrameHostImpl*>(
          navigation_handle()->GetRenderFrameHost()));
  while (parent) {
    if (!csp_context.IsAllowedByCsp(
            network::mojom::CSPDirectiveName::FrameAncestors,
            parent->GetLastCommittedOrigin().GetURL(),
            navigation_handle()->WasServerRedirect(),
            true /* is_response_check */, empty_source_location,
            network::CSPContext::CheckCSPDisposition::CHECK_ALL_CSP,
            navigation_handle()->IsFormSubmission())) {
      return CheckResult::BLOCK;
    }
    parent = ParentOrOuterDelegate(parent);
  }

  return CheckResult::PROCEED;
}

// When the embedder requires the use of Content Security Policy via Embedded
// Enforcement, framed documents must either
// 1) Use the 'allow-csp-from' header to opt-into enforcement.
// 2) Enforce its own CSP that subsumes the required CSP.
// Framed documents that fail to do either of these will be blocked.
//
// See:
// - https://w3c.github.io/webappsec-cspee/#required-csp-header
// - https://w3c.github.io/webappsec-cspee/#allow-csp-from-header
AncestorThrottle::CheckResult
AncestorThrottle::EvaluateCSPEmbeddedEnforcement() {
  if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE))
    return CheckResult::PROCEED;

  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  if (request->IsInMainFrame()) {
    // We enforce CSPEE only for frames, not for portals.
    return CheckResult::PROCEED;
  }

  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      navigation_handle()->GetRenderFrameHost());

  if (!request->required_csp())
    return CheckResult::PROCEED;

  const network::mojom::AllowCSPFromHeaderValuePtr& allow_csp_from =
      request->response()->parsed_headers->allow_csp_from;
  if (AllowsBlanketEnforcementOfRequiredCSP(
          frame->GetParent()->GetLastCommittedOrigin(),
          navigation_handle()->GetURL(), allow_csp_from)) {
    // Enforce the required csps on the frame by passing them down to blink
    request->ForceCSPForResponse(request->required_csp()->header->header_value);
    return CheckResult::PROCEED;
  }

  std::string sanitized_blocked_url =
      navigation_handle()->GetRedirectChain().front().GetOrigin().spec();
  if (allow_csp_from && allow_csp_from->is_error_message()) {
    frame->GetParent()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf("The value of the 'Allow-CSP-From' response header "
                           "returned by %s is invalid: %s",
                           sanitized_blocked_url.c_str(),
                           allow_csp_from->get_error_message().c_str()));
  }
  if (network::Subsumes(
          *request->required_csp(),
          request->response()->parsed_headers->content_security_policy,
          url::Origin::Create(navigation_handle()->GetURL()))) {
    return CheckResult::PROCEED;
  }

  frame->GetParent()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(
          "Refused to display '%s' in a frame. The embedder requires it to "
          "enforce the following Content Security Policy: '%s'. However, the "
          "frame neither accepts that policy using the Allow-CSP-From header "
          "nor delivers a Content Security Policy which is at least as strong "
          "as that one.",
          sanitized_blocked_url.c_str(),
          request->required_csp()->header->header_value.c_str()));

  return CheckResult::BLOCK;
}

// static
bool AncestorThrottle::AllowsBlanketEnforcementOfRequiredCSP(
    const url::Origin& request_origin,
    const GURL& response_url,
    const network::mojom::AllowCSPFromHeaderValuePtr& allow_csp_from) {
  if (response_url.SchemeIs(url::kAboutScheme) ||
      response_url.SchemeIs(url::kDataScheme) || response_url.SchemeIsFile() ||
      response_url.SchemeIsFileSystem() || response_url.SchemeIsBlob()) {
    return true;
  }

  if (request_origin.IsSameOriginWith(url::Origin::Create(response_url))) {
    return true;
  }

  if (!allow_csp_from)
    return false;

  if (allow_csp_from->is_allow_star()) {
    return true;
  }
  if (allow_csp_from->is_origin() &&
      request_origin.IsSameOriginWith(allow_csp_from->get_origin())) {
    return true;
  }

  return false;
}

AncestorThrottle::HeaderDisposition AncestorThrottle::ParseXFrameOptionsHeader(
    const net::HttpResponseHeaders* headers,
    std::string* header_value) {
  DCHECK(header_value);
  if (!headers)
    return HeaderDisposition::NONE;

  // Process the 'X-Frame-Options header as per Section 2 of RFC7034:
  // https://tools.ietf.org/html/rfc7034#section-2
  //
  // Note that we do not support the 'ALLOW-FROM' value, and we special-case
  // the invalid "ALLOWALL" value due to its prevalance in the wild.
  HeaderDisposition result = HeaderDisposition::NONE;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, "x-frame-options", &value)) {
    HeaderDisposition current = HeaderDisposition::INVALID;

    base::StringPiece trimmed =
        base::TrimWhitespaceASCII(value, base::TRIM_ALL);
    if (!header_value->empty())
      header_value->append(", ");
    header_value->append(trimmed.as_string());

    if (base::LowerCaseEqualsASCII(trimmed, "deny"))
      current = HeaderDisposition::DENY;
    else if (base::LowerCaseEqualsASCII(trimmed, "allowall"))
      current = HeaderDisposition::ALLOWALL;
    else if (base::LowerCaseEqualsASCII(trimmed, "sameorigin"))
      current = HeaderDisposition::SAMEORIGIN;
    else
      current = HeaderDisposition::INVALID;

    if (result == HeaderDisposition::NONE)
      result = current;
    else if (result != current)
      result = HeaderDisposition::CONFLICT;
  }

  return result;
}

}  // namespace content
