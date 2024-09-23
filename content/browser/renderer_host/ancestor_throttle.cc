// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ancestor_throttle.h"

#include "base/feature_list.h"
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
#include "content/browser/renderer_host/render_frame_host_csp_context.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

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

// From a RenderFrameHost |frame|, return its parent. This escapes FencedFrames
// that allow for information inflow, but does not escape nested WebContents.
// This returns nullptr for the top-level document and fenced frame roots (if
// information inflow is not allowed for the fenced frame). |request| is only
// supplied when |frame| is the frame being navigated. The FencedFrameProperties
// has not been installed in |frame|'s FrameTreeNode yet. Instead, we look at
// the one attached to the NavigationRequest.
RenderFrameHostImpl* GetParentForFrameAncestors(NavigationRequest* request,
                                                RenderFrameHostImpl* frame) {
  bool allows_information_inflow = false;
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    if (request) {
      allows_information_inflow =
          !request->GetFencedFrameProperties().has_value() ||
          request->GetFencedFrameProperties()->allows_information_inflow();
    } else {
      // We are in one of the navigated frame's ancestors.
      allows_information_inflow =
          !frame->frame_tree_node()->HasFencedFrameProperties() ||
          frame->frame_tree_node()
              ->GetFencedFrameProperties()
              ->allows_information_inflow();
    }
  } else {
    allows_information_inflow = !frame->IsFencedFrameRoot();
  }

  if (!allows_information_inflow && request &&
      base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    request->AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "'CSP frame-ancestors' and 'X-Frame-Options' directives will not look "
        "past a fenced frame boundary if created with an API that disallows "
        "information inflow, such as Protected Audience.");
  }

  return allows_information_inflow ? frame->GetParentOrOuterDocument()
                                   : nullptr;
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle> AncestorThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return base::WrapUnique(new AncestorThrottle(handle));
}

AncestorThrottle::~AncestorThrottle() {}

NavigationThrottle::ThrottleCheckResult
AncestorThrottle::WillRedirectRequest() {
  // During a redirect, we don't know which RenderFrameHost we'll end up in,
  // so we can't log reliably to the console. We should be able to work around
  // this iff we decide to ship the redirect-blocking behavior, but for now
  // we'll just skip the console-logging bits to collect metrics.
  NavigationThrottle::ThrottleCheckResult result = ProcessResponseImpl(
      LoggingDisposition::DO_NOT_LOG_TO_CONSOLE, false /* is_response_check */);

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

  if (request->IsInOutermostMainFrame()) {
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

  if (EvaluateEmbeddingOptIn(logging) == CheckResult::BLOCK)
    return NavigationThrottle::BLOCK_RESPONSE;

  return NavigationThrottle::PROCEED;
}

const char* AncestorThrottle::GetNameForLogging() {
  return "AncestorThrottle";
}

AncestorThrottle::AncestorThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

void AncestorThrottle::ParseXFrameOptionsError(
    const net::HttpResponseHeaders* headers,
    network::mojom::XFrameOptionsValue disposition) {
  DCHECK(disposition == network::mojom::XFrameOptionsValue::kConflict ||
         disposition == network::mojom::XFrameOptionsValue::kInvalid);
  DCHECK(headers);

  std::string value;
  headers->GetNormalizedHeader("X-Frame-Options", &value);

  std::string message;
  if (disposition == network::mojom::XFrameOptionsValue::kConflict) {
    message = base::StringPrintf(
        "Refused to display '%s' in a frame because it set multiple "
        "'X-Frame-Options' headers with conflicting values "
        "('%s'). Falling back to 'deny'.",
        url::Origin::Create(navigation_handle()->GetURL())
            .GetURL()
            .spec()
            .c_str(),
        value.c_str());
  } else {
    message = base::StringPrintf(
        "Invalid 'X-Frame-Options' header encountered when loading '%s': "
        "'%s' is not a recognized directive. The header will be ignored.",
        url::Origin::Create(navigation_handle()->GetURL())
            .GetURL()
            .spec()
            .c_str(),
        value.c_str());
  }

  AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                      std::move(message));
}

void AncestorThrottle::ConsoleErrorEmbeddingRequiresOptIn() {
  DCHECK(base::FeatureList::IsEnabled(features::kEmbeddingRequiresOptIn));
  std::string message = base::StringPrintf(
      "Refused to display '%s' in a frame: It did not opt-into cross-origin "
      "embedding by setting either an 'X-Frame-Options' header, or a "
      "'Content-Security-Policy' header containing a 'frame-ancestors' "
      "directive.",
      url::Origin::Create(navigation_handle()->GetURL())
          .GetURL()
          .spec()
          .c_str());

  AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                      std::move(message));
}

void AncestorThrottle::ConsoleErrorXFrameOptions(
    network::mojom::XFrameOptionsValue disposition) {
  DCHECK(disposition == network::mojom::XFrameOptionsValue::kDeny ||
         disposition == network::mojom::XFrameOptionsValue::kSameOrigin);
  std::string message = base::StringPrintf(
      "Refused to display '%s' in a frame because it set 'X-Frame-Options' "
      "to '%s'.",
      url::Origin::Create(navigation_handle()->GetURL())
          .GetURL()
          .spec()
          .c_str(),
      disposition == network::mojom::XFrameOptionsValue::kDeny ? "deny"
                                                               : "sameorigin");

  AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                      std::move(message));
}

void AncestorThrottle::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    std::string message) {
  NavigationRequest::From(navigation_handle())
      ->AddDeferredConsoleMessage(level, std::move(message));
}

AncestorThrottle::CheckResult AncestorThrottle::EvaluateXFrameOptions(
    LoggingDisposition logging) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  network::mojom::XFrameOptionsValue disposition =
      request->response()->parsed_headers->xfo;

  // If 'X-Frame-Options' would potentially block the response, check whether
  // the 'frame-ancestors' CSP directive should take effect instead. See
  // https://www.w3.org/TR/CSP/#frame-ancestors-and-frame-options
  if (disposition != network::mojom::XFrameOptionsValue::kNone &&
      disposition != network::mojom::XFrameOptionsValue::kAllowAll &&
      HeadersContainFrameAncestorsCSP(request->response()->parsed_headers)) {
    return CheckResult::PROCEED;
  }

  switch (disposition) {
    case network::mojom::XFrameOptionsValue::kConflict:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseXFrameOptionsError(request->GetResponseHeaders(), disposition);
      return CheckResult::BLOCK;

    case network::mojom::XFrameOptionsValue::kInvalid:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ParseXFrameOptionsError(request->GetResponseHeaders(), disposition);
      // TODO(mkwst): Consider failing here, especially if we end up shipping
      // a new default behavior which requires embedees to explicitly opt-in
      // to being embedded: https://crbug.com/1153274.
      return CheckResult::PROCEED;

    case network::mojom::XFrameOptionsValue::kDeny:
      if (logging == LoggingDisposition::LOG_TO_CONSOLE)
        ConsoleErrorXFrameOptions(disposition);
      return CheckResult::BLOCK;

    case network::mojom::XFrameOptionsValue::kSameOrigin: {
      // Block the request when any ancestor is not same-origin.
      // We enforce XFrameOptions in the outer documents, but not for
      // embedders/GuestViews.
      RenderFrameHostImpl* parent = GetParentForFrameAncestors(
          request, request->frame_tree_node()->current_frame_host());
      url::Origin current_origin =
          url::Origin::Create(navigation_handle()->GetURL());
      while (parent) {
        if (!parent->GetLastCommittedOrigin().IsSameOriginWith(
                current_origin)) {
          if (logging == LoggingDisposition::LOG_TO_CONSOLE)
            ConsoleErrorXFrameOptions(disposition);
          return CheckResult::BLOCK;
        }
        parent = GetParentForFrameAncestors(nullptr, parent);
      }
      return CheckResult::PROCEED;
    }

    case network::mojom::XFrameOptionsValue::kNone:
      return CheckResult::PROCEED;
    case network::mojom::XFrameOptionsValue::kAllowAll:
      return CheckResult::PROCEED;
  }
}

AncestorThrottle::CheckResult AncestorThrottle::EvaluateEmbeddingOptIn(
    LoggingDisposition logging) {
  // If the proposal in https://github.com/mikewest/embedding-requires-opt-in is
  // enabled, a response will be blocked unless it's explicitly opted-into
  // being embeddable via 'X-Frame-Options'/'frame-ancestors', or is same-origin
  // with its ancestors.
  // We enforce frame-ancestors in the outer documents, but not for
  // embedders/GuestViews.
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  if (request->response()->parsed_headers->xfo ==
          network::mojom::XFrameOptionsValue::kNone &&
      !HeadersContainFrameAncestorsCSP(request->response()->parsed_headers)) {
    RenderFrameHostImpl* parent = GetParentForFrameAncestors(
        request, request->frame_tree_node()->current_frame_host());
    while (parent) {
      if (!parent->GetLastCommittedOrigin().IsSameOriginWith(
              navigation_handle()->GetURL())) {
        GetContentClient()->browser()->LogWebFeatureForCurrentPage(
            parent, blink::mojom::WebFeature::
                        kEmbeddedCrossOriginFrameWithoutFrameAncestorsOrXFO);

        if (!base::FeatureList::IsEnabled(features::kEmbeddingRequiresOptIn))
          return CheckResult::PROCEED;

        if (logging == LoggingDisposition::LOG_TO_CONSOLE)
          ConsoleErrorEmbeddingRequiresOptIn();

        return CheckResult::BLOCK;
      }
      parent = GetParentForFrameAncestors(nullptr, parent);
    }
  }
  return CheckResult::PROCEED;
}

AncestorThrottle::CheckResult AncestorThrottle::EvaluateFrameAncestors(
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        content_security_policy) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  // TODO(lfg): If the initiating document is known and correspond to the
  // navigating frame's current document, consider using:
  // navigation_request().common_params().source_location here instead.
  auto empty_source_location = network::mojom::SourceLocation::New();

  // Check CSP frame-ancestors against every parent. We enforce frame-ancestors
  // in the outer documents (except for fenced frames created under certain
  // conditions), but not for embedders or GuestViews.
  RenderFrameHostImpl* parent = GetParentForFrameAncestors(
      request, static_cast<RenderFrameHostImpl*>(
                   navigation_handle()->GetRenderFrameHost()));

  while (parent) {
    // CSP violations (if any) are reported via the disallowed ancestor of the
    // navigated frame (because while the throttle runs the navigation hasn't
    // committed yet and the target frame might not yet have a URLLoaderFactory
    // that could be used to report the violation).
    // See also https://crbug.com/1111049.
    network::CSPCheckResult result =
        RenderFrameHostCSPContext(parent).IsAllowedByCsp(
            content_security_policy,
            network::mojom::CSPDirectiveName::FrameAncestors,
            parent->GetLastCommittedOrigin().GetURL(),
            GURL(),  // url_before_redirects is ignored for frame-ancestors
            navigation_handle()->WasServerRedirect(), empty_source_location,
            network::CSPContext::CheckCSPDisposition::CHECK_ALL_CSP,
            navigation_handle()->IsFormSubmission());
    if (result.WouldBlockIfWildcardDoesNotMatchWs()) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          parent,
          blink::mojom::WebFeature::kCspWouldBlockIfWildcardDoesNotMatchWs);
    }
    if (result.WouldBlockIfWildcardDoesNotMatchFtp()) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          parent,
          blink::mojom::WebFeature::kCspWouldBlockIfWildcardDoesNotMatchFtp);
    }
    if (!result) {
      return CheckResult::BLOCK;
    }
    parent = GetParentForFrameAncestors(nullptr, parent);
  }

  return CheckResult::PROCEED;
}

}  // namespace content
