// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/critical_client_hints_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"

namespace {

void LogCriticalCHStatus(CriticalCHRestart status) {
  UMA_HISTOGRAM_ENUMERATION("ClientHints.CriticalCHRestart", status);
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

void CriticalClientHintsThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (redirected_)
    return;

  if (!response_head->parsed_headers ||
      !response_head->parsed_headers->accept_ch ||
      !response_head->parsed_headers->critical_ch)
    return;

  // Ensure that only hints in the accept-ch header are examined
  blink::WebEnabledClientHints hints;

  for (const auto& hint : response_head->parsed_headers->accept_ch.value())
    hints.SetIsEnabled(hint, true);

  std::vector<network::mojom::WebClientHintsType> critical_hints;
  for (const auto& hint : response_head->parsed_headers->critical_ch.value())
    if (hints.IsEnabled(hint))
      critical_hints.push_back(hint);

  if (critical_hints.empty())
    return;

  LogCriticalCHStatus(CriticalCHRestart::kHeaderPresent);

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);

  if (AreCriticalHintsMissing(response_url, frame_tree_node,
                              client_hint_delegate_, critical_hints)) {
    redirected_ = true;
    auto parsed = ParseAndPersistAcceptCHForNagivation(
        response_url, response_head->parsed_headers, context_,
        client_hint_delegate_, frame_tree_node);

    net::HttpRequestHeaders modified_headers;
    AddNavigationRequestClientHintsHeaders(
        response_url, &modified_headers, context_, client_hint_delegate_,
        frame_tree_node->navigation_request()->GetIsOverridingUserAgent(),
        frame_tree_node);

    LogCriticalCHStatus(CriticalCHRestart::kNavigationRestarted);

    delegate_->RestartWithModifiedHeadersNow(modified_headers);
  }
}
}  // namespace content
