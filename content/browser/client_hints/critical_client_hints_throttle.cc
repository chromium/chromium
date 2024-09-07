// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/critical_client_hints_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace {

using ::network::mojom::WebClientHintsType;

void LogCriticalCHStatus(CriticalCHRestart status) {
  base::UmaHistogramEnumeration("ClientHints.CriticalCHRestart", status);
}

}  // namespace

namespace content {

CriticalClientHintsThrottle::CriticalClientHintsThrottle(
    BrowserContext* context,
    ClientHintsControllerDelegate* client_hint_delegate,
    FrameTreeNodeId frame_tree_node_id)
    : context_(context),
      client_hint_delegate_(client_hint_delegate),
      frame_tree_node_id_(frame_tree_node_id) {
  LogCriticalCHStatus(CriticalCHRestart::kNavigationStarted);
}

CriticalClientHintsThrottle::~CriticalClientHintsThrottle() = default;

void CriticalClientHintsThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  response_url_ = request->url;
  initial_request_headers_ = request->headers;
}

void CriticalClientHintsThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset) {
  DCHECK_EQ(response_url, response_url_);
  MaybeRestartWithHints(response_head, restart_with_url_reset);
}

void CriticalClientHintsThrottle::BeforeWillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  MaybeRestartWithHints(response_head, restart_with_url_reset);
  response_url_ = redirect_info->new_url;
}

void CriticalClientHintsThrottle::MaybeRestartWithHints(
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset) {
  if (!base::FeatureList::IsEnabled(features::kCriticalClientHint))
    return;
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);

  // Measure any usage of the header whether or not we take action on it.
  auto* ukm_recorder = ukm::UkmRecorder::Get();
  if (response_head.parsed_headers && frame_tree_node &&
      frame_tree_node->navigation_request()) {
    if (response_head.parsed_headers->critical_ch) {
      for (const WebClientHintsType hint :
           response_head.parsed_headers->critical_ch.value()) {
        ukm::builders::ClientHints_CriticalCHHeaderUsage(
            frame_tree_node->navigation_request()->GetNextPageUkmSourceId())
            .SetType(static_cast<int64_t>(hint))
            .Record(ukm_recorder->Get());
      }
    }
    if (response_head.parsed_headers->accept_ch) {
      for (const WebClientHintsType hint :
           response_head.parsed_headers->accept_ch.value()) {
        ukm::builders::ClientHints_AcceptCHHeaderUsage(
            frame_tree_node->navigation_request()->GetNextPageUkmSourceId())
            .SetType(static_cast<int64_t>(hint))
            .Record(ukm_recorder->Get());
      }
    }
  }

  if (!response_head.parsed_headers ||
      !response_head.parsed_headers->accept_ch ||
      !response_head.parsed_headers->critical_ch)
    return;

  url::Origin response_origin = url::Origin::Create(response_url_);

  // Only restart once per-Origin (per navigation)
  if (restarted_origins_.contains(response_origin))
    return;

  if (!ShouldAddClientHints(response_origin, frame_tree_node,
                            client_hint_delegate_)) {
    return;
  }

  // Ensure that only hints in the accept-ch header are examined
  blink::EnabledClientHints hints;
  for (const WebClientHintsType hint :
       response_head.parsed_headers->accept_ch.value())
    hints.SetIsEnabled(hint, true);

  std::vector<WebClientHintsType> critical_hints;
  for (const WebClientHintsType hint :
       response_head.parsed_headers->critical_ch.value())
    if (hints.IsEnabled(hint))
      critical_hints.push_back(hint);

  if (critical_hints.empty())
    return;

  LogCriticalCHStatus(CriticalCHRestart::kHeaderPresent);

  if (!AreCriticalHintsMissing(response_origin, frame_tree_node,
                               client_hint_delegate_, critical_hints)) {
    return;
  }

  ParseAndPersistAcceptCHForNavigation(response_origin,
                                       response_head.parsed_headers,
                                       response_head.headers.get(), context_,
                                       client_hint_delegate_, frame_tree_node);
  restarted_origins_.insert(response_origin);

  net::HttpRequestHeaders modified_headers;
  // TODO(crbug.com/40175866): If the frame tree node doesn't have an associated
  // navigation_request (e.g. a service worker request) it might not override
  // the user agent correctly.
  if (frame_tree_node) {
    AddNavigationRequestClientHintsHeaders(
        response_origin, &modified_headers, context_, client_hint_delegate_,
        frame_tree_node->navigation_request()->is_overriding_user_agent(),
        frame_tree_node,
        frame_tree_node->navigation_request()
            ->commit_params()
            .frame_policy.container_policy);
  } else {
    AddPrefetchNavigationRequestClientHintsHeaders(
        response_origin, &modified_headers, context_, client_hint_delegate_,
        /*is_ua_override_on=*/false, /*is_javascript_enabled=*/true);
  }

  // If a client hint header is not in the original request,
  // restart the request.
  for (auto modified_header : modified_headers.GetHeaderVector()) {
    if (!initial_request_headers_.HasHeader(modified_header.key)) {
      LogCriticalCHStatus(CriticalCHRestart::kNavigationRestarted);
      delegate_->DidRestartForCriticalClientHint();
      *restart_with_url_reset = RestartWithURLReset(true);
      return;
    }
  }
}
}  // namespace content
