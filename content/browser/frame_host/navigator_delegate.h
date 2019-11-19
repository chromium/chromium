// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_

#include "base/strings/string16.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class FrameTreeNode;
class NavigationHandle;
class RenderFrameHostImpl;
struct LoadCommittedDetails;
struct OpenURLParams;

// A delegate API used by Navigator to notify its embedder of navigation
// related events.
class CONTENT_EXPORT NavigatorDelegate {
 public:
  // Called when a navigation started. The same NavigationHandle will be
  // provided for events related to the same navigation.
  virtual void DidStartNavigation(NavigationHandle* navigation_handle) {}

  // Called when a navigation was redirected.
  virtual void DidRedirectNavigation(NavigationHandle* navigation_handle) {}

  // Called when the navigation is about to be committed in a renderer.
  virtual void ReadyToCommitNavigation(NavigationHandle* navigation_handle) {}

  // Called when the navigation finished: it was either committed or canceled
  // before commit.  Note that |navigation_handle| will be destroyed at the end
  // of this call.
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle) {}

  // TODO(clamy): all methods below that are related to navigation
  // events should go away in favor of the ones above.

  // Document load in |render_frame_host| failed.
  virtual void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                                    const GURL& url,
                                    int error_code,
                                    const base::string16& error_description) {}

  // Handles post-navigation tasks in navigation BEFORE the entry has been
  // committed to the NavigationController.
  virtual void DidNavigateMainFramePreCommit(bool navigation_is_within_page) {}

  // Handles post-navigation tasks in navigation AFTER the entry has been
  // committed to the NavigationController. Note that the NavigationEntry is
  // not provided since it may be invalid/changed after being committed. The
  // NavigationController's last committed entry is for this navigation.
  virtual void DidNavigateMainFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}
  virtual void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}

  virtual void SetMainFrameMimeType(const std::string& mime_type) {}
  virtual bool CanOverscrollContent() const;

  // Notification to the Navigator embedder that navigation state has
  // changed. This method corresponds to
  // WebContents::NotifyNavigationStateChanged.
  virtual void NotifyChangedNavigationState(InvalidateTypes changed_flags) {}

  // Notifies the Navigator embedder that a navigation to the pending
  // NavigationEntry has started in the browser process.
  virtual void DidStartNavigationToPendingEntry(const GURL& url,
                                                ReloadType reload_type) {}

  // Opens a URL with the given parameters. See PageNavigator::OpenURL, which
  // this is an alias of.
  virtual WebContents* OpenURL(const OpenURLParams& params) = 0;

  // Returns whether to continue a navigation that needs to transfer to a
  // different process between the load start and commit.
  virtual bool ShouldTransferNavigation(bool is_main_frame_navigation);

  // Returns the overriden user agent string if it's set.
  virtual const std::string& GetUserAgentOverride() = 0;

  // Returns whether we should override the user agent in new tabs, e.g., for
  // Android Webview's popup window when current entry.
  virtual bool ShouldOverrideUserAgentInNewTabs() = 0;

  // A RenderFrameHost in the specified |frame_tree_node| started loading a new
  // document. This correponds to Blink's notion of the throbber starting.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  virtual void DidStartLoading(FrameTreeNode* frame_tree_node,
                               bool to_different_document) {}

  // A document stopped loading. This corresponds to Blink's notion of the
  // throbber stopping.
  virtual void DidStopLoading() {}

  // The load progress was changed.
  virtual void DidChangeLoadProgress() {}

  // Returns the NavigationThrottles to add to this navigation. Normally these
  // are defined by the content/ embedder, except in the case of interstitials
  // where no NavigationThrottles are added to the navigation.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>
  CreateThrottlesForNavigation(NavigationHandle* navigation_handle);

  // Called at the start of the navigation to get opaque data the embedder
  // wants to see passed to the corresponding URLRequest on the IO thread.
  // In the case of a navigation to an interstitial, no call will be made to the
  // embedder and |nullptr| is returned.
  virtual std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
