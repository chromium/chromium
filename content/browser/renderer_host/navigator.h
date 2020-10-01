// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/navigation_client.mojom.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/impression.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace base {
class TimeTicks;
}

namespace network {
class ResourceRequestBody;
}

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class NavigationControllerImpl;
class NavigatorDelegate;
class NavigationEntryImpl;
class NavigationRequest;
class PrefetchedSignedExchangeCache;
class RenderFrameHostImpl;
class WebBundleHandleTracker;
struct LoadCommittedDetails;
struct UrlInfo;

// Navigator is responsible for performing navigations in nodes of the
// FrameTree. Its lifetime is bound to the FrameTree.
class CONTENT_EXPORT Navigator {
 public:
  Navigator(NavigationControllerImpl* navigation_controller,
            NavigatorDelegate* delegate);
  ~Navigator();

  // This method verifies that a navigation to |url| doesn't commit into a WebUI
  // process if it is not allowed to. Callers of this method should take one of
  // two actions if the method returns false:
  // * When called from browser process logic (e.g. NavigationRequest), this
  //   indicates issues with the navigation logic and the browser process must
  //   be terminated to avoid security issues.
  // * If the codepath is processing an IPC message from a renderer process,
  //   then the renderer process is misbehaving and must be terminated.
  // TODO(nasko): Remove the is_renderer_initiated_check parameter when callers
  // of this method are migrated to use CHECK instead of DumpWithoutCrashing.
  static WARN_UNUSED_RESULT bool CheckWebUIRendererDoesNotDisplayNormalURL(
      RenderFrameHostImpl* render_frame_host,
      const UrlInfo& url_info,
      bool is_renderer_initiated_check);

  static bool ShouldIgnoreIncomingRendererRequest(
      const NavigationRequest* ongoing_navigation_request,
      bool has_user_gesture);

  // Returns the delegate of this Navigator.
  NavigatorDelegate* GetDelegate();

  // Returns the NavigationController associated with this Navigator.
  NavigationController* GetController();

  // Notifications coming from the RenderFrameHosts ----------------------------

  // The RenderFrameHostImpl has failed to load the document.
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code);

  // The RenderFrameHostImpl has committed a navigation. The Navigator is
  // responsible for resetting |navigation_request| at the end of this method
  // and should not attempt to keep it alive. Note: it is possible that
  // |navigation_request| is not the NavigationRequest stored in the
  // RenderFrameHost that just committed. This happens for example when a
  // same-page navigation commits while another navigation is ongoing. The
  // Navigator should use the NavigationRequest provided by this method and not
  // attempt to access the RenderFrameHost's NavigationsRequests.
  void DidNavigate(RenderFrameHostImpl* render_frame_host,
                   const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   std::unique_ptr<NavigationRequest> navigation_request,
                   bool was_within_same_document);

  // Called on a newly created subframe during a history navigation. The browser
  // process looks up the corresponding FrameNavigationEntry for the new frame
  // navigates it in the correct process. Returns false if the
  // FrameNavigationEntry can't be found or the navigation fails.
  bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>*
          navigation_client);

  // Navigation requests -------------------------------------------------------

  // Called by the NavigationController to cause the Navigator to navigate to
  // |navigation_request|. The NavigationController should be called back with
  // RendererDidNavigate on success or DiscardPendingEntry on failure. The
  // callbacks should be called in a future iteration of the message loop.
  void Navigate(std::unique_ptr<NavigationRequest> request,
                ReloadType reload_type,
                RestoreType restore_type);

  // The RenderFrameHostImpl has received a request to open a URL with the
  // specified |disposition|.
  void RequestOpenURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const GlobalFrameRoutingId& initiator_routing_id,
      const base::Optional<url::Origin>& initiator_origin,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture,
      blink::TriggeringEventInfo triggering_event_info,
      const std::string& href_translate,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      const base::Optional<Impression>& impression);

  // Called when a document requests a navigation in another document through a
  // RenderFrameProxy. If |method| is "POST", then |post_body| needs to specify
  // the request body, otherwise |post_body| should be null.
  void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const GlobalFrameRoutingId& initiator_routing_id,
      const url::Origin& initiator_origin,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      NavigationDownloadPolicy download_policy,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool has_user_gesture,
      const base::Optional<Impression>& impression);

  // Called after BeforeUnloadCompleted callback is invoked from the renderer.
  // If |frame_tree_node| has a NavigationRequest waiting for the renderer
  // response, then the request is either started or canceled, depending on the
  // value of |proceed|.
  void BeforeUnloadCompleted(FrameTreeNode* frame_tree_node,
                             bool proceed,
                             const base::TimeTicks& proceed_time);

  // Used to start a new renderer-initiated navigation, following a
  // BeginNavigation IPC from the renderer.
  void OnBeginNavigation(
      FrameTreeNode* frame_tree_node,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      std::unique_ptr<WebBundleHandleTracker> web_bundle_handle_tracker);

  // Used to restart a navigation that was thought to be same-document in
  // cross-document mode.
  void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Cancel a NavigationRequest for |frame_tree_node|.
  void CancelNavigation(FrameTreeNode* frame_tree_node);

  // Called when the network stack started handling the navigation request
  // so that the |timestamp| when it happened can be recorded into an histogram.
  // The |url| is used to verify we're tracking the correct navigation.
  // TODO(carlosk): Remove the URL parameter and rename this method to better
  // suit naming conventions.
  void LogResourceRequestTime(base::TimeTicks timestamp, const GURL& url);

  // Called to record the time it took to execute the beforeunload hook for the
  // current navigation.
  void LogBeforeUnloadTime(base::TimeTicks renderer_before_unload_start_time,
                           base::TimeTicks renderer_before_unload_end_time,
                           base::TimeTicks before_unload_sent_time);

  NavigationControllerImpl* controller() { return controller_; }

 private:
  friend class NavigatorTestWithBrowserSideNavigation;

  // Holds data used to track browser side navigation metrics.
  struct NavigationMetricsData;

  void RecordNavigationMetrics(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      SiteInstance* site_instance);

  // Called when a renderer initiated navigation has started. Returns the
  // pending NavigationEntry to be used. Either null or a new one owned
  // NavigationController.
  NavigationEntryImpl* GetNavigationEntryForRendererInitiatedNavigation(
      const mojom::CommonNavigationParams& common_params,
      FrameTreeNode* frame_tree_node);

  // Called to record the time it took to execute beforeunload handlers for
  // renderer-inititated navigations. It records the time it took to execute
  // beforeunload handlers in the renderer process before sending the
  // BeginNavigation IPC.
  void LogRendererInitiatedBeforeUnloadTime(
      base::TimeTicks renderer_before_unload_start_time,
      base::TimeTicks renderer_before_unload_end_time);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this Navigator.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be nullptr in tests.
  NavigatorDelegate* delegate_;

  std::unique_ptr<Navigator::NavigationMetricsData> navigation_data_;

  DISALLOW_COPY_AND_ASSIGN(Navigator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_
