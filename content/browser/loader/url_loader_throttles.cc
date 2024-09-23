// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_loader_throttles.h"

#include <optional>

#include "base/feature_list.h"
#include "base/memory/safe_ref.h"
#include "components/variations/net/omnibox_url_loader_throttle.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/client_hints/critical_client_hints_throttle.h"
#include "content/browser/origin_trials/critical_origin_trials_throttle.h"
#include "content/browser/reduce_accept_language/reduce_accept_language_throttle.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webid/webid_utils.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/web_identity.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          request, browser_context, wc_getter, navigation_ui_data,
          frame_tree_node_id, navigation_id);
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(&throttles);
  // TODO(crbug.com/40135370): Consider whether we want to use the WebContents
  // to determine the value for variations::Owner. Alternatively, this is the
  // browser side, and we might be fine with Owner::kUnknown.
  variations::VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
      browser_context->GetVariationsClient(), &throttles);

  ClientHintsControllerDelegate* client_hint_delegate =
      browser_context->GetClientHintsControllerDelegate();
  // TODO(bokan): How to handle client hints in a fenced frame is still an open
  // question, see:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/permission_document_policies.md#ua-client-hints-open-question
  if (base::FeatureList::IsEnabled(features::kCriticalClientHint) &&
      net::HttpUtil::IsMethodSafe(request.method) &&
      request.is_outermost_main_frame && client_hint_delegate) {
    throttles.push_back(std::make_unique<CriticalClientHintsThrottle>(
        browser_context, client_hint_delegate, frame_tree_node_id));
  }

  // Creating a throttle only for outermost main frames to persist the reduced
  // accept language for an origin and to restart requests if needed, due to
  // language negotiation.
  if (base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage)) {
    ReduceAcceptLanguageControllerDelegate* reduce_accept_lang_delegate =
        browser_context->GetReduceAcceptLanguageControllerDelegate();
    OriginTrialsControllerDelegate* origin_trials_delegate =
        browser_context->GetOriginTrialsControllerDelegate();
    if (request.is_outermost_main_frame && reduce_accept_lang_delegate) {
      throttles.push_back(std::make_unique<ReduceAcceptLanguageThrottle>(
          *reduce_accept_lang_delegate, origin_trials_delegate,
          frame_tree_node_id));
    }
  }

  // frame_tree_node_id may be invalid if we are loading the first frame
  // of the tab.
  FrameTreeNode* frame_tree_node = nullptr;
  if (frame_tree_node_id) {
    frame_tree_node = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  }

  // Handle Critical Origin Trial headers if the context supports it and this
  // is a navigation request.
  OriginTrialsControllerDelegate* origin_trials_delegate =
      browser_context->GetOriginTrialsControllerDelegate();
  if (origin_trials_delegate &&
      CriticalOriginTrialsThrottle::IsNavigationRequest(request)) {
    // Critical Origin Trials may restart the network request, so only allow on
    // safe methods, since the origin trials in question may change request
    // headers or other aspects of the network request. We want to avoid servers
    // making any changes twice as a result of the duplicate request, and if
    // headers are changed, any idempotent method is still allowed to make
    // further changes to server state.
    if (net::HttpUtil::IsMethodSafe(request.method) && origin_trials_delegate) {
      std::optional<url::Origin> top_origin = std::nullopt;
      // The throttle should only use a top-frame origin for partitioning if
      // this is not the outermost frame.
      if (frame_tree_node && frame_tree_node->GetParentOrOuterDocument()) {
        top_origin = frame_tree_node->GetParentOrOuterDocument()
                         ->GetOutermostMainFrame()
                         ->GetLastCommittedOrigin();
      }
      std::optional<ukm::SourceId> source_id =
          navigation_id.has_value()
              ? std::make_optional(ukm::ConvertToSourceId(
                    navigation_id.value(), ukm::SourceIdType::NAVIGATION_ID))
              : std::nullopt;
      throttles.push_back(std::make_unique<CriticalOriginTrialsThrottle>(
          *origin_trials_delegate, std::move(top_origin), source_id));
    }
  }

  auto throttle = MaybeCreateIdentityUrlLoaderThrottle(base::BindRepeating(
      webid::SetIdpSigninStatus, browser_context, frame_tree_node_id));
  if (throttle) {
    throttles.push_back(std::move(throttle));
  }

  return throttles;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CreateContentBrowserURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    FrameTreeNodeId frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottlesForKeepAlive(
          request, browser_context, wc_getter, frame_tree_node_id);
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(&throttles);
  // TODO(crbug.com/40135370): Consider whether we want to use the WebContents
  // to determine the value for variations::Owner. Alternatively, this is the
  // browser side, and we might be fine with Owner::kUnknown.
  variations::VariationsURLLoaderThrottle::AppendThrottleIfNeeded(
      browser_context->GetVariationsClient(), &throttles);

  auto throttle = MaybeCreateIdentityUrlLoaderThrottle(base::BindRepeating(
      webid::SetIdpSigninStatus, browser_context, frame_tree_node_id));
  if (throttle) {
    throttles.push_back(std::move(throttle));
  }

  return throttles;
}
}  // namespace content
