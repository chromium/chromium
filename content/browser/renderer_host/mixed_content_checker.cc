// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mixed_content_checker.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {
namespace {

// Tells whether in-browser mixed content checker should cancel prerendering
// pages when they are loading a mixed content fetch keepalive request.
bool ShouldFetchKeepAliveCancelPrerenderingOnMixedContent() {
  return GetFieldTrialParamByFeatureAsBool(
      blink::features::kKeepAliveInBrowserMigration,
      "cancel_prerendering_on_mixed_content", /*default_value=*/true);
}

bool IsSecureScheme(const std::string& scheme) {
  return base::Contains(url::GetSecureSchemes(), scheme);
}

// Should return the same value as `SecurityOrigin::IsLocal()` and
// `blink::SchemeRegistry::ShouldTreatURLSchemeAsCorsEnabled()`.
bool ShouldTreatURLSchemeAsCorsEnabled(const GURL& url) {
  return base::Contains(url::GetCorsEnabledSchemes(), url.scheme());
}

// Should return the same value as the resource URL checks result from
// `IsUrlPotentiallyTrustworthy()` made inside
// `blink::MixedContentChecker::IsMixedContent()`.
bool IsUrlPotentiallySecure(const GURL& url) {
  return network::IsUrlPotentiallyTrustworthy(url);
}

// This method should return the same results as
// `blink::SchemeRegistry::ShouldTreatURLSchemeAsRestrictingMixedContent()`.
bool DoesOriginSchemeRestrictMixedContent(const url::Origin& origin) {
  return origin.GetTupleOrPrecursorTupleIfOpaque().scheme() ==
         url::kHttpsScheme;
}

// This mirrors `blink::MixedContentChecker::IsMixedContent()`.
bool IsMixedContent(const url::Origin& origin, const GURL& url) {
  return !IsUrlPotentiallySecure(url) &&
         DoesOriginSchemeRestrictMixedContent(origin);
}

// This mirrors `blink::MixedContentChecker::InWhichFrameIsContentMixed()` but
// without reporting to renderer.
// Unlike the other `InWhichFrameIsContentMixed` in this file, this function
// should only be called to find the mixed content frame for fetch keepalive
// requests, and it does not record any blink::mojom::WebFeature when
// identifying mixed content frames.
RenderFrameHostImpl* InWhichFrameIsContentMixedForFetchKeepAlive(
    RenderFrameHostImpl* initiator_frame,
    const GURL& url) {
  // The caller may provide nullptr `initiator_frame` if a fetch keepalive
  // request stay alive until after the frame of its document is destroyed. In
  // such case, there is no enough information to calculate whether the request
  // is mixed content or not.
  // See also the mirrored function in Blink, which does not calculate mixed
  // content for frameless call.
  if (!initiator_frame) {
    return nullptr;
  }

  // Check the main frame first.
  RenderFrameHostImpl* main_frame = initiator_frame->GetOutermostMainFrame();
  if (IsMixedContent(main_frame->GetLastCommittedOrigin(), url)) {
    return main_frame;
  }

  if (IsMixedContent(initiator_frame->GetLastCommittedOrigin(), url)) {
    return initiator_frame;
  }

  // No mixed content, no problem.
  return nullptr;
}

void UpdateRendererOnMixedContentFound(NavigationRequest* navigation_request,
                                       const GURL& mixed_content_url,
                                       bool was_allowed,
                                       bool for_redirect) {
  // TODO(carlosk): the root node should never be considered as being/having
  // mixed content for now. Once/if the browser should also check form submits
  // for mixed content than this will be allowed to happen and this DCHECK
  // should be updated.
  DCHECK(!navigation_request->IsInOutermostMainFrame());

  RenderFrameHostImpl* rfh =
      navigation_request->frame_tree_node()->current_frame_host();
  DCHECK(!navigation_request->GetRedirectChain().empty());
  GURL url_before_redirects = navigation_request->GetRedirectChain()[0];
  rfh->GetAssociatedLocalFrame()->MixedContentFound(
      mixed_content_url, navigation_request->GetURL(),
      navigation_request->request_context_type(), was_allowed,
      url_before_redirects, for_redirect,
      navigation_request->common_params().source_location.Clone());
}

// Updates the renderer about any Blink feature usage.
void MaybeSendBlinkFeatureUsageReport(
    NavigationHandle& navigation_handle,
    std::set<blink::mojom::WebFeature>& mixed_content_features) {
  if (!mixed_content_features.empty()) {
    NavigationRequest* request = NavigationRequest::From(&navigation_handle);
    RenderFrameHostImpl* rfh = request->frame_tree_node()->current_frame_host();
    rfh->GetAssociatedLocalFrame()->ReportBlinkFeatureUsage(
        std::vector<blink::mojom::WebFeature>(mixed_content_features.begin(),
                                              mixed_content_features.end()));
    mixed_content_features.clear();
  }
}

// Records basic mixed content "feature" usage when any kind of mixed content
// is found.
//
// Based off of `blink::MixedContentChecker::Count()`.
void ReportBasicMixedContentFeatures(
    blink::mojom::RequestContextType request_context_type,
    blink::mojom::MixedContentContextType mixed_content_context_type,
    std::set<blink::mojom::WebFeature>& mixed_content_features) {
  mixed_content_features.insert(blink::mojom::WebFeature::kMixedContentPresent);

  // Report any blockable content.
  if (mixed_content_context_type ==
      blink::mojom::MixedContentContextType::kBlockable) {
    mixed_content_features.insert(
        blink::mojom::WebFeature::kMixedContentBlockable);
    return;
  }

  // Note: as there's no mixed content checks for sub-resources on the browser
  // side there should only be a subset of `RequestContextType` values that
  // could ever be found here.
  blink::mojom::WebFeature feature;
  switch (request_context_type) {
    case blink::mojom::RequestContextType::INTERNAL:
      feature = blink::mojom::WebFeature::kMixedContentInternal;
      break;
    case blink::mojom::RequestContextType::PREFETCH:
      feature = blink::mojom::WebFeature::kMixedContentPrefetch;
      break;

    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::FAVICON:
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::PLUGIN:
    case blink::mojom::RequestContextType::VIDEO:
    default:
      NOTREACHED_IN_MIGRATION()
          << "RequestContextType has value " << request_context_type
          << " and has MixedContentContextType of "
          << mixed_content_context_type;
      return;
  }
  mixed_content_features.insert(feature);
}

}  // namespace

MixedContentChecker::MixedContentChecker() = default;
MixedContentChecker::~MixedContentChecker() = default;

bool MixedContentChecker::ShouldBlockNavigation(
    NavigationHandle& navigation_handle,
    bool for_redirect) {
  NavigationRequest* request = NavigationRequest::From(&navigation_handle);
  FrameTreeNode* node = request->frame_tree_node();

  // Find the parent frame where mixed content is characterized, if any.
  RenderFrameHostImpl* mixed_content_frame =
      InWhichFrameIsContentMixed(node, request->GetURL());
  if (!mixed_content_frame) {
    MaybeSendBlinkFeatureUsageReport(navigation_handle,
                                     navigation_mixed_content_features_);
    return false;
  }

  // From this point on we know this is not a main frame navigation and that
  // there is mixed content. Now let's decide if it's OK to proceed with it.

  ReportBasicMixedContentFeatures(request->request_context_type(),
                                  request->mixed_content_context_type(),
                                  navigation_mixed_content_features_);

  bool should_report_to_renderer = false;
  bool should_block = ShouldBlockInternal(
      mixed_content_frame, node, request->GetURL(), for_redirect,
      /*cancel_prerendering=*/true, request->mixed_content_context_type(),
      &navigation_mixed_content_features_, &should_report_to_renderer);

  if (should_report_to_renderer) {
    UpdateRendererOnMixedContentFound(
        request, mixed_content_frame->GetLastCommittedURL(),
        /*was_allowed=*/!should_block, for_redirect);
    MaybeSendBlinkFeatureUsageReport(navigation_handle,
                                     navigation_mixed_content_features_);
  }
  return should_block;
}

// static
bool MixedContentChecker::ShouldBlockInternal(
    RenderFrameHostImpl* mixed_content_frame,
    FrameTreeNode* node,
    const GURL& url,
    bool for_redirect,
    bool cancel_prerendering,
    blink::mojom::MixedContentContextType mixed_content_context_type,
    std::set<blink::mojom::WebFeature>* mixed_content_features,
    bool* should_report_to_renderer) {
  CHECK(mixed_content_frame);

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client/embedder checks in order to prevent degrading
  // the site's security UI.
  // TODO(crbug.com/40220595): Instead of checking
  // `node->IsInFencedFrameTree()`, set insecure_request_policy to block mixed
  // content requests in a fenced frame tree and change
  // `InWhichFrameIsContentMixed()` to not cross the frame tree boundary.
  bool block_all_mixed_content =
      ((mixed_content_frame->frame_tree_node()
            ->current_replication_state()
            .insecure_request_policy &
        blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent) !=
       blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) ||
      node->IsInFencedFrameTree();
  const auto& prefs = mixed_content_frame->GetOrCreateWebPreferences();
  bool strict_mode =
      prefs.strict_mixed_content_checking || block_all_mixed_content;

  // Do not treat non-webby schemes as mixed content when loaded in subframes.
  // Navigations to non-webby schemes cannot return data to the browser, so
  // insecure content will not be run or displayed to the user as a result of
  // loading a non-webby scheme. It is potentially dangerous to navigate to a
  // non-webby scheme (e.g., the page could deliver a malicious payload to a
  // vulnerable native application), but loading a non-webby scheme is no more
  // dangerous in this respect than navigating the main frame to the non-webby
  // scheme directly. See https://crbug.com/621131.
  //
  // TODO(crbug.com/40109927): decide whether CORS-enabled is really the
  // right way to draw this distinction.
  if (!ShouldTreatURLSchemeAsCorsEnabled(url)) {
    return false;
  }

  // Cancel the prerendering page to prevent the problems that can be the
  // logging UMA, UKM and calling `DidChangeVisibleSecurityState()` through this
  // throttle.
  if (cancel_prerendering &&
      mixed_content_frame->CancelPrerendering(
          PrerenderCancellationReason(PrerenderFinalStatus::kMixedContent))) {
    return true;
  }

  bool allowed = false;
  RenderFrameHostDelegate* frame_host_delegate =
      node->current_frame_host()->delegate();
  switch (mixed_content_context_type) {
    case blink::mojom::MixedContentContextType::kOptionallyBlockable:
      allowed = !strict_mode;
      if (allowed) {
        frame_host_delegate->PassiveInsecureContentFound(url);
        node->frame_tree().controller().ssl_manager()->DidDisplayMixedContent();
      }
      break;

    case blink::mojom::MixedContentContextType::kBlockable: {
      // Note: from the renderer side implementation it seems like we don't need
      // to care about reporting
      // `blink::WebFeature::kBlockableMixedContentInSubframeBlocked` because it
      // is only triggered for sub-resources which are not checked for in the
      // browser.
      bool should_ask_delegate =
          !strict_mode && (!prefs.strictly_block_blockable_mixed_content ||
                           prefs.allow_running_insecure_content);
      allowed = should_ask_delegate &&
                frame_host_delegate->ShouldAllowRunningInsecureContent(
                    prefs.allow_running_insecure_content,
                    mixed_content_frame->GetLastCommittedOrigin(), url);
      if (allowed) {
        const GURL& origin_url =
            mixed_content_frame->GetLastCommittedOrigin().GetURL();
        mixed_content_frame->OnDidRunInsecureContent(origin_url, url);
        if (mixed_content_features) {
          mixed_content_features->insert(
              blink::mojom::WebFeature::kMixedContentBlockableAllowed);
        }
      }
      break;
    }

    case blink::mojom::MixedContentContextType::kShouldBeBlockable:
      allowed = !strict_mode;
      if (allowed) {
        node->frame_tree().controller().ssl_manager()->DidDisplayMixedContent();
      }
      break;

    case blink::mojom::MixedContentContextType::kNotMixedContent:
      NOTREACHED_IN_MIGRATION();
      break;
  };

  if (should_report_to_renderer) {
    *should_report_to_renderer = true;
  }

  return !allowed;
}

RenderFrameHostImpl* MixedContentChecker::InWhichFrameIsContentMixed(
    FrameTreeNode* node,
    const GURL& url) {
  // Main frame navigations cannot be mixed content. But, fenced frame
  // navigations should be considered as well because it can be mixed content.

  // TODO(carlosk): except for form submissions which might be supported in the
  // future.
  if (!node->GetParentOrOuterDocument()) {
    return nullptr;
  }

  // There's no mixed content if any of these are true:
  // - The navigated URL is potentially secure.
  // - Neither the root nor parent frames have secure origins.
  // This next section diverges in how the Blink version is implemented but
  // should get to the same results. Especially where
  // `blink::MixedContentChecker::IsMixedContent()` calls exist, here they are
  // partially fulfilled here and partially replaced by
  // `DoesOriginSchemeRestrictMixedContent()`.
  RenderFrameHostImpl* mixed_content_frame = nullptr;
  RenderFrameHostImpl* parent = node->GetParentOrOuterDocument();
  RenderFrameHostImpl* root = parent->GetOutermostMainFrame();

  if (!IsUrlPotentiallySecure(url)) {
    // TODO(carlosk): we might need to check more than just the immediate parent
    // and the root. See https://crbug.com/623486.

    // Checks if the root and then the immediate parent frames' origins are
    // secure.
    if (DoesOriginSchemeRestrictMixedContent(root->GetLastCommittedOrigin())) {
      mixed_content_frame = root;
    } else if (DoesOriginSchemeRestrictMixedContent(
                   parent->GetLastCommittedOrigin())) {
      mixed_content_frame = parent;
    }
  }

  // Note: The code below should behave the same way as the two calls to
  // `MeasureStricterVersionOfIsMixedContent()` from inside
  // `blink::MixedContentChecker::InWhichFrameIsContentMixed()`.
  if (mixed_content_frame) {
    // We're currently only checking for mixed content in `https://*` contexts.
    // What about other "secure" contexts the `SchemeRegistry` knows about?
    // We'll use this method to measure the occurrence of non-webby mixed
    // content to make sure we're not breaking the world without realizing it.
    if (mixed_content_frame->GetLastCommittedOrigin().scheme() !=
        url::kHttpsScheme) {
      navigation_mixed_content_features_.insert(
          blink::mojom::WebFeature::
              kMixedContentInNonHTTPSFrameThatRestrictsMixedContent);
    }
  } else if (!network::IsUrlPotentiallyTrustworthy(url) &&
             (IsSecureScheme(root->GetLastCommittedOrigin().scheme()) ||
              IsSecureScheme(parent->GetLastCommittedOrigin().scheme()))) {
    navigation_mixed_content_features_.insert(
        blink::mojom::WebFeature::
            kMixedContentInSecureFrameThatDoesNotRestrictMixedContent);
  }
  return mixed_content_frame;
}

// static
bool MixedContentChecker::ShouldBlockFetchKeepAlive(
    RenderFrameHostImpl* initiator_frame,
    const GURL& url,
    bool for_redirect) {
  // A fetch keepalive request's RequestContextType is one of the following:
  // - RequestContextType::FETCH,
  // - RequestContextType::BEACON,
  // - RequestContextType::ATTRIBUTION_SRC,
  // which all maps to kBlockable.
  // See also `blink::MixedContent::ContextTypeFromRequestContext()`.
  constexpr auto kMixedContentContextType =
      blink::mojom::MixedContentContextType::kBlockable;

  RenderFrameHostImpl* mixed_content_frame =
      InWhichFrameIsContentMixedForFetchKeepAlive(initiator_frame, url);
  if (!mixed_content_frame) {
    return false;
  }

  return ShouldBlockInternal(
      mixed_content_frame, initiator_frame->frame_tree_node(), url,
      for_redirect,
      /*cancel_prerendering=*/
      ShouldFetchKeepAliveCancelPrerenderingOnMixedContent(),
      kMixedContentContextType);
}

// static
bool MixedContentChecker::IsMixedContentForTesting(const GURL& origin_url,
                                                   const GURL& url) {
  const url::Origin origin = url::Origin::Create(origin_url);
  return IsMixedContent(origin, url);
}

}  // namespace content
