// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_DELEGATE_H_

#include "content/common/navigation_client.mojom.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/trust_token_access_details.h"

class GURL;

namespace blink {
struct UserAgentOverride;
}  // namespace blink

namespace network::mojom {
class SharedDictionaryAccessDetails;
}  // namespace network::mojom

namespace content {

class CommitDeferringCondition;
class NavigationHandle;
class NavigationRequest;
class RenderFrameHostImpl;
struct LoadCommittedDetails;
struct OpenURLParams;

// A delegate API used by Navigator to notify its embedder of navigation
// related events.
class NavigatorDelegate {
 public:
  // Called when a navigation started. The same NavigationHandle will be
  // provided for events related to the same navigation.
  virtual void DidStartNavigation(NavigationHandle* navigation_handle) = 0;

  // Called when a navigation was redirected.
  virtual void DidRedirectNavigation(NavigationHandle* navigation_handle) = 0;

  // Called when the navigation is about to be committed in a renderer.
  virtual void ReadyToCommitNavigation(NavigationHandle* navigation_handle) = 0;

  // Called when the navigation finished: it was either committed or canceled
  // before commit.  Note that |navigation_handle| will be destroyed at the end
  // of this call.
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle) = 0;

  // Called when the navigation gets cancelled before it even starts (i.e.,
  // the respective `NavigationRequest::StartNavigation()`). This can happen
  // when the user decides to not leave the current page by interacting with the
  // BeforeUnload dialog. Can also happen if `BeginNavigationImpl()` reaches an
  // early out. If the navigation never starts, `DidFinishNavigation()` won't be
  // fired. Use this API to observe the destruction of such a navigation
  // request.
  virtual void DidCancelNavigationBeforeStart(
      NavigationHandle* navigation_handle) = 0;

  // TODO(clamy): all methods below that are related to navigation
  // events should go away in favor of the ones above.

  // Handles post-navigation tasks in navigation BEFORE the entry has been
  // committed to the NavigationController.
  virtual void DidNavigateMainFramePreCommit(
      NavigationHandle* navigation_handle,
      bool navigation_is_within_page) = 0;

  // Handles post-navigation tasks in navigation AFTER the entry has been
  // committed to the NavigationController. Note that the NavigationEntry is
  // not provided since it may be invalid/changed after being committed. The
  // NavigationController's last committed entry is for this navigation.
  virtual void DidNavigateMainFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details) = 0;
  virtual void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details) = 0;

  // Called when the NavigationHandleTiming associated with `navigation_handle`
  // has been updated. See the comment at
  // `WebContentsObserver::DidUpdateNavigationHandleTiming()` for more details.
  virtual void DidUpdateNavigationHandleTiming(
      NavigationHandle* navigation_handle) = 0;

  // Notification to the Navigator embedder that navigation state has
  // changed. This method corresponds to
  // WebContents::NotifyNavigationStateChanged.
  virtual void NotifyChangedNavigationState(InvalidateTypes changed_flags) = 0;

  // Opens a URL with the given parameters. See PageNavigator::OpenURL, which
  // this is an alias of.
  virtual WebContents* OpenURL(const OpenURLParams& params,
                               base::OnceCallback<void(NavigationHandle&)>
                                   navigation_handle_callback) = 0;

  // Returns whether to continue a navigation that needs to transfer to a
  // different process between the load start and commit.
  virtual bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation) = 0;

  // Returns the overridden user agent string if it's set.
  virtual const blink::UserAgentOverride& GetUserAgentOverride() = 0;

  // Returns the value to use for NavigationEntry::IsOverridingUserAgent() for
  // a renderer initiated navigation.
  virtual bool ShouldOverrideUserAgentForRendererInitiatedNavigation() = 0;

  // Returns the NavigationThrottles to add to this navigation. Normally these
  // are defined by the content/ embedder, except in the case of interstitials
  // where no NavigationThrottles are added to the navigation.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>
  CreateThrottlesForNavigation(NavigationHandle* navigation_handle) = 0;

  // Returns commit deferring conditions to add to this navigation.
  virtual std::vector<std::unique_ptr<CommitDeferringCondition>>
  CreateDeferringConditionsForNavigationCommit(
      NavigationHandle& navigation_handle,
      CommitDeferringCondition::NavigationType type) = 0;

  // Called at the start of the navigation to get opaque data the embedder
  // wants to see passed to the corresponding URLRequest on the IO thread.
  // In the case of a navigation to an interstitial, no call will be made to the
  // embedder and |nullptr| is returned.
  virtual std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle) = 0;

  // Called when a navigation accessed ServiceWorker to check if it should be
  // handled by the ServiceWorker or not.
  virtual void OnServiceWorkerAccessed(NavigationHandle* navigation,
                                       const GURL& scope,
                                       AllowServiceWorkerResult allowed) = 0;

  // Called when a network request issued by this navigation set or read a
  // cookie.
  virtual void OnCookiesAccessed(NavigationHandle* navigation,
                                 const CookieAccessDetails& details) = 0;

  // Called when a network request issued by this navigation accesses a Trust
  // Token.
  virtual void OnTrustTokensAccessed(
      NavigationHandle* navigation,
      const TrustTokenAccessDetails& details) = 0;

  // Called when a network request issued by this navigation accesses a shared
  // dictionary.
  virtual void OnSharedDictionaryAccessed(
      NavigationHandle* navigation,
      const network::mojom::SharedDictionaryAccessDetails& details) = 0;

  // Does a global walk of the session history and all committed/pending-commit
  // origins, and registers origins that match |origin| to their respective
  // BrowsingInstances. |navigation_request_to_exclude| allows the
  // NavigationRequest that initiates this process to avoid marking itself as
  // non-opted-in before it gets the chance to opt-in.
  virtual void RegisterExistingOriginAsHavingDefaultIsolation(
      const url::Origin& origin,
      NavigationRequest* navigation_request_to_exclude) = 0;

  // Request to capture the content area as a bitmap. Return false if the
  // embedder is not overlaying any content on the current navigation entry's
  // Document. Return true if a bitmap will be captured. Callback must be
  // dispatched asynchronously (with an empty bitmap if the capture fails,
  // e.g. not enough memory) if this returns true.
  virtual bool MaybeCopyContentAreaAsBitmap(
      base::OnceCallback<void(const SkBitmap&)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_DELEGATE_H_
