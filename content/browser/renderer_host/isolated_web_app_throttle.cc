// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/isolated_web_app_throttle.h"

#include "base/feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#endif

namespace content {

namespace {

// Stores the origin of the Isolated Web App that a WebContents is bound to.
// Every WebContents will have an instance of this class assigned to it during
// its first navigation, which will determine whether the WebContents hosts an
// Isolated Web App for its lifetime. Note that activating an alternative frame
// tree (e.g. preloading or portals) will NOT override this state.
class WebContentsIsolationInfo
    : public WebContentsUserData<WebContentsIsolationInfo> {
 public:
  ~WebContentsIsolationInfo() override = default;

  bool is_isolated_application() { return isolated_origin_.has_value(); }

  const url::Origin& origin() {
    DCHECK(is_isolated_application());
    return isolated_origin_.value();
  }

 private:
  friend class WebContentsUserData<WebContentsIsolationInfo>;
  explicit WebContentsIsolationInfo(WebContents* web_contents,
                                    std::optional<url::Origin> isolated_origin)
      : WebContentsUserData<WebContentsIsolationInfo>(*web_contents),
        isolated_origin_(isolated_origin) {}

  std::optional<url::Origin> isolated_origin_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsIsolationInfo);

bool IsNavigatingToIsolatedApplication(NavigationHandle* handle) {
  return SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      handle->GetWebContents()->GetBrowserContext(), handle->GetURL());
}

}  // namespace

// static
std::unique_ptr<IsolatedWebAppThrottle>
IsolatedWebAppThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  BrowserContext* browser_context = NavigationRequest::From(handle)
                                        ->frame_tree_node()
                                        ->navigator()
                                        .controller()
                                        .GetBrowserContext();

  if (AreIsolatedWebAppsEnabled(browser_context)) {
    return std::make_unique<IsolatedWebAppThrottle>(handle);
  }
  return nullptr;
}

IsolatedWebAppThrottle::IsolatedWebAppThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

IsolatedWebAppThrottle::~IsolatedWebAppThrottle() = default;

NavigationThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::WillStartRequest() {
  bool requests_app_isolation =
      IsNavigatingToIsolatedApplication(navigation_handle());
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  dest_origin_ = navigation_request->GetTentativeOriginAtRequestTime();

  // If this is the first navigation in this WebContents, save the isolation
  // state to validate future navigations.
  auto* web_contents_isolation_info = WebContentsIsolationInfo::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!web_contents_isolation_info) {
    WebContentsIsolationInfo::CreateForWebContents(
        navigation_handle()->GetWebContents(),
        requests_app_isolation ? std::make_optional(dest_origin_)
                               : std::nullopt);
  }

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (!frame_tree_node->is_on_initial_empty_document()) {
    prev_origin_ = frame_tree_node->current_origin();
  }

  return MaybeThrottleNavigationTransition(requests_app_isolation,
                                           ThrottleAction::BLOCK_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::WillRedirectRequest() {
  // On redirects, the old destination origin becomes the new previous origin.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  prev_origin_ = dest_origin_;
  dest_origin_ = navigation_request->GetTentativeOriginAtRequestTime();

  return MaybeThrottleNavigationTransition(
      IsNavigatingToIsolatedApplication(navigation_handle()),
      ThrottleAction::BLOCK_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  auto* assigned_rfh = navigation_request->GetRenderFrameHost();
  // Allow downloads and 204s (for these GetOriginToCommit returns nullopt).
  if (!assigned_rfh) {
    return ThrottleAction::PROCEED;
  }

  // Update |dest_origin_| to point to the final origin, which may have changed
  // since the last WillStartRequest/WillRedirectRequest call.
  dest_origin_ = navigation_request->GetOriginToCommit().value();
  const WebExposedIsolationInfo& assigned_isolation_info =
      assigned_rfh->GetSiteInstance()->GetWebExposedIsolationInfo();
  return MaybeThrottleNavigationTransition(
      assigned_isolation_info.is_isolated_application(),
      ThrottleAction::BLOCK_RESPONSE);
}

bool IsolatedWebAppThrottle::OpenUrlExternal(const GURL& url) {
  ui::PageTransition transition =
      navigation_handle()->GetRedirectChain().size() > 1
          ? ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT
          : ui::PageTransition::PAGE_TRANSITION_LINK;
#if BUILDFLAG(IS_CHROMEOS)
  // The default browser can't be changed in ChromeOS, so just open the URL
  // directly.
  // TODO(crbug.com/40830234): Should we set the referrer?
  OpenURLParams params(url, Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB, transition,
                       /*is_renderer_initiated=*/false);
  params.open_app_window_if_possible = true;
  GetContentClient()->browser()->OpenURL(
      navigation_handle()->GetStartingSiteInstance(), params,
      base::DoNothing());
  return true;
#else
  NavigationRequest* navigation_request =
      NavigationRequest::From(navigation_handle());
  const FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory;
  return GetContentClient()->browser()->HandleExternalProtocol(
      url,
      base::BindRepeating(
          [](const FrameTreeNodeId frame_tree_node_id) {
            return WebContents::FromFrameTreeNodeId(frame_tree_node_id);
          },
          frame_tree_node->frame_tree_node_id()),
      frame_tree_node->frame_tree_node_id(),
      navigation_request->GetNavigationUIData(),
      /*is_primary_main_frame=*/true, /*is_in_fenced_frame_tree=*/false,
      network::mojom::WebSandboxFlags::kNone, transition,
      navigation_request->HasUserGesture(),
      /*initiating_origin=*/std::nullopt,
      /*initiator_document=*/nullptr, navigation_request->GetIsolationInfo(),
      &loader_factory);
#endif
}

NavigationThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::MaybeThrottleNavigationTransition(
    bool dest_needs_apps_isolation,
    ThrottleAction block_action) {
  auto* web_contents_isolation_info = WebContentsIsolationInfo::FromWebContents(
      navigation_handle()->GetWebContents());
  DCHECK(web_contents_isolation_info);

  // Block navigations into Isolated Web Apps (IWA) from non-IWA contexts.
  if (!web_contents_isolation_info->is_isolated_application()) {
    // non-IWA => non-IWA is ok, but non-IWA => IWA is not.
    return dest_needs_apps_isolation ? block_action : ThrottleAction::PROCEED;
  }

  // We want the following origin checks to be a bit more permissive than
  // usual. In particular, if the isolation, previous, or destination origins
  // are opaque, we want to use their precursor tuple for "origin" comparisons.
  // This lets us allow navigations to/from data, error, or web bundle URLs
  // that originate from the same precursor URLs. Other rules may block these
  // navigations, but for the purpose of this throttle, these navigations are
  // valid.
  const url::SchemeHostPort& web_contents_isolation_tuple =
      web_contents_isolation_info->origin().GetTupleOrPrecursorTupleIfOpaque();
  const url::SchemeHostPort& dest_tuple =
      dest_origin_.GetTupleOrPrecursorTupleIfOpaque();

  // Invariant: we're currently in an IWA Web Contents.
  CHECK(web_contents_isolation_info->is_isolated_application());

  // Handle main frame navigations.
  if (navigation_handle()->IsInMainFrame()) {
    // If the main frame tries to leave the app's origin, cancel the
    // navigation and open the URL in the systems' default application.
    if (dest_tuple != web_contents_isolation_tuple) {
      OpenUrlExternal(navigation_handle()->GetURL());
      return ThrottleAction::CANCEL;
    }

    // It's very unlikely that `dest_needs_apps_isolation` is false at this
    // stage. However, to gracefully handle this, the navigation will be aborted
    // if that's the case.
    // TODO(crbug.com/417403902): Investigate this.
    return dest_needs_apps_isolation ? ThrottleAction::PROCEED : block_action;
  } else {
    // Handle iframe navigations.
    CHECK(!navigation_handle()->IsInMainFrame());

    // Iframes are allowed to leave the app's origin.
    if (dest_tuple != web_contents_isolation_tuple) {
      return ThrottleAction::PROCEED;
    }

    // Block renderer-initiated iframe navigations into the app that were
    // initiated by a non-app frame. This ensures that all iframe navigations
    // into the app come from the app itself.
    if (navigation_handle()->IsRendererInitiated() && prev_origin_ &&
        prev_origin_->GetTupleOrPrecursorTupleIfOpaque() !=
            web_contents_isolation_tuple) {
      // Allow the navigation if it was initiated by the app, meaning it has a
      // trusted destination URL. This only applies to the initial request, as
      // redirect locations come from outside the app.
      if (navigation_handle()->GetRedirectChain().size() == 1 &&
          navigation_handle()->GetInitiatorOrigin().has_value()) {
        const url::SchemeHostPort& initiator_tuple =
            navigation_handle()
                ->GetInitiatorOrigin()
                .value()
                .GetTupleOrPrecursorTupleIfOpaque();
        if (initiator_tuple == web_contents_isolation_tuple) {
          return ThrottleAction::PROCEED;
        }
      }
      return block_action;
    }

    // Block iframe navigations to the app's origin if the parent frame
    // doesn't belong to the app. This prevents non-app frames from having
    // access to an app frame.
    const url::SchemeHostPort& parent_tuple =
        navigation_handle()
            ->GetParentFrame()
            ->GetLastCommittedOrigin()
            .GetTupleOrPrecursorTupleIfOpaque();
    if (parent_tuple != web_contents_isolation_tuple) {
      return block_action;
    }

    // Allow iframe same-origin navigations to blob: and data: URLs
    // (cross-origin iframe navigation are already allowed and handled further
    // up as part of the `dest_tuple != web_contents_isolation_tuple`
    // condition).
    if (navigation_handle()->GetURL().SchemeIs(url::kDataScheme) ||
        navigation_handle()->GetURL().SchemeIsBlob()) {
      return ThrottleAction::PROCEED;
    }

    // It's very unlikely that `dest_needs_apps_isolation` is false at this
    // stage. However, to gracefully handle this, the navigation will be aborted
    // if that's the case.
    // TODO(crbug.com/417403902): Investigate this.
    return dest_needs_apps_isolation ? ThrottleAction::PROCEED : block_action;
  }
}

const char* IsolatedWebAppThrottle::GetNameForLogging() {
  return "IsolatedWebAppThrottle";
}

}  // namespace content
