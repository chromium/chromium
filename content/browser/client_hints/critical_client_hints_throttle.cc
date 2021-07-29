// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/critical_client_hints_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/http/http_util.h"
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

void LogAcceptCHFrameStatus(AcceptCHFrameRestart status) {
  base::UmaHistogramEnumeration("ClientHints.AcceptCHFrame", status);
}

}  // namespace

namespace content {

CriticalClientHintsThrottle::CriticalClientHintsThrottle(
    BrowserContext* context,
    ClientHintsControllerDelegate* client_hint_delegate,
    int frame_tree_node_id)
    : context_(context),
      client_hint_delegate_(client_hint_delegate),
      frame_tree_node_id_(frame_tree_node_id) {
  LogCriticalCHStatus(CriticalCHRestart::kNavigationStarted);
}

void CriticalClientHintsThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  if (!base::FeatureList::IsEnabled(features::kCriticalClientHint))
    return;

  if (!response_head.parsed_headers ||
      !response_head.parsed_headers->accept_ch ||
      !response_head.parsed_headers->critical_ch)
    return;

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

  // TODO(crbug.com/1228536): This isn't really used, just in the other call to
  // the same function. A refactor is probably in order.
  net::HttpRequestHeaders modified_headers;
  if (ShouldRestartWithHints(response_url, critical_hints, modified_headers)) {
    LogCriticalCHStatus(CriticalCHRestart::kNavigationRestarted);
    ParseAndPersistAcceptCHForNavigation(
        response_url, response_head.parsed_headers, context_,
        client_hint_delegate_,
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_));
    delegate_->RestartWithURLResetAndFlags(/*additional_load_flags=*/0);
  }
}

void CriticalClientHintsThrottle::HandleAcceptCHFrameReceived(
    const GURL& url,
    const std::vector<WebClientHintsType>& accept_ch_frame) {
  if (accept_ch_frame_redirect_ ||
      !base::FeatureList::IsEnabled(network::features::kAcceptCHFrame)) {
    return;
  }

  LogAcceptCHFrameStatus(AcceptCHFrameRestart::kFramePresent);

  net::HttpRequestHeaders modified_headers;

  if (ShouldRestartWithHints(url, accept_ch_frame, modified_headers)) {
    accept_ch_frame_redirect_ = true;
    LogAcceptCHFrameStatus(AcceptCHFrameRestart::kNavigationRestarted);
    delegate_->RestartWithModifiedHeadersNow(modified_headers);
  }
}

bool CriticalClientHintsThrottle::ShouldRestartWithHints(
    const GURL& response_url,
    const std::vector<WebClientHintsType>& hints,
    net::HttpRequestHeaders& modified_headers) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);

  if (!AreCriticalHintsMissing(response_url, frame_tree_node,
                               client_hint_delegate_, hints)) {
    return false;
  }

  client_hint_delegate_->SetAdditionalClientHints(hints);
  // TODO(crbug.com/1195034): If the frame tree node doesn't have an associated
  // navigation_request (e.g. a service worker request) it might not override
  // the user agent correctly.
  if (frame_tree_node) {
    AddNavigationRequestClientHintsHeaders(
        response_url, &modified_headers, context_, client_hint_delegate_,
        frame_tree_node->navigation_request()->is_overriding_user_agent(),
        frame_tree_node,
        frame_tree_node->navigation_request()
            ->commit_params()
            .frame_policy.container_policy);
  } else {
    AddPrefetchNavigationRequestClientHintsHeaders(
        response_url, &modified_headers, context_, client_hint_delegate_,
        /*is_ua_override_on=*/false, /*is_javascript_enabled=*/true);
  }
  client_hint_delegate_->ClearAdditionalClientHints();
  return true;
}
}  // namespace content
