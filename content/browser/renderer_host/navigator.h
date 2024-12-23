// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/common/content_export.h"
#include "content/common/navigation_client.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/frame/triggering_event_info.mojom-shared.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace base {
class TimeTicks;
}

namespace network {
class ResourceRequestBody;
}

namespace content {

class BrowserContext;
class FrameNavigationEntry;
class FrameTree;
class FrameTreeNode;
class NavigationControllerDelegate;
class NavigationEntryImpl;
class NavigationRequest;
class NavigatorDelegate;
class PrefetchedSignedExchangeCache;
class RenderFrameHostImpl;
struct LoadCommittedDetails;
struct UrlInfo;

// Navigator is responsible for performing navigations in nodes of the
// FrameTree. Its lifetime is bound to the FrameTree.
class CONTENT_EXPORT Navigator {
 public:
  Navigator(BrowserContext* browser_context,
            FrameTree& frame_tree,
            NavigatorDelegate* delegate,
            NavigationControllerDelegate* navigation_controller_delegate);

  Navigator(const Navigator&) = delete;
  Navigator& operator=(const Navigator&) = delete;

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
  [[nodiscard]] static bool CheckWebUIRendererDoesNotDisplayNormalURL(
      RenderFrameHostImpl* render_frame_host,
      const UrlInfo& url_info,
      bool is_renderer_initiated_check);

  static bool ShouldIgnoreIncomingRendererRequest(
      const NavigationRequest* ongoing_navigation_request,
      bool has_user_gesture);

  // Returns the delegate of this Navigator.
  NavigatorDelegate* GetDelegate();

  // Notifications coming from the RenderFrameHosts ----------------------------

  // The RenderFrameHostImpl has committed a navigation. The Navigator is
  // responsible for resetting |navigation_request| at the end of this method
  // and should not attempt to keep it alive. Note: it is possible that
  // |navigation_request| is not the NavigationRequest stored in the
  // RenderFrameHost that just committed. This happens for example when a
  // same-page navigation commits while another navigation is ongoing. The
  // Navigator should use the NavigationRequest provided by this method and not
  // attempt to access the RenderFrameHost's NavigationsRequests.
  void DidNavigate(RenderFrameHostImpl* render_frame_host,
                   const mojom::DidCommitProvisionalLoadParams& params,
                   std::unique_ptr<NavigationRequest> navigation_request,
                   bool was_within_same_document);

  // Called on a newly created subframe during a history navigation. The browser
  // process looks up the corresponding FrameNavigationEntry for the new frame
  // navigates it in the correct process. Returns false if the
  // FrameNavigationEntry can't be found or the navigation fails.
  bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client,
      blink::LocalFrameToken initiator_frame_token,
      int initiator_process_id);

  // Navigation requests -------------------------------------------------------

  // Called by the NavigationController to cause the Navigator to navigate to
  // |navigation_request|. The NavigationController should be called back with
  // RendererDidNavigate on success or DiscardPendingEntry on failure. The
  // callbacks should be called in a future iteration of the message loop.
  void Navigate(std::unique_ptr<NavigationRequest> request,
                ReloadType reload_type);

  // The RenderFrameHostImpl has received a request to open a URL with the
  // specified |disposition|.
  void RequestOpenURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const blink::LocalFrameToken* initiator_frame_token,
      int initiator_process_id,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<GURL>& initiator_base_url,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture,
      blink::mojom::TriggeringEventInfo triggering_event_info,
      const std::string& href_translate,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      const std::optional<blink::Impression>& impression,
      bool has_rel_opener);

  // Called when a document requests a navigation in another document through a
  // `blink::RemoteFrame`. If `method` is "POST", then `post_body` needs to
  // specify the request body, otherwise `post_body` should be null.
  void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const blink::LocalFrameToken* initiator_frame_token,
      int initiator_process_id,
      const url::Origin& initiator_origin,
      const std::optional<GURL>& initiator_base_url,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      blink::NavigationDownloadPolicy download_policy,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      network::mojom::SourceLocationPtr source_location,
      bool has_user_gesture,
      bool is_form_submission,
      const std::optional<blink::Impression>& impression,
      blink::mojom::NavigationInitiatorActivationAndAdStatus
          initiator_activation_and_ad_status,
      base::TimeTicks navigation_start_time,
      bool is_embedder_initiated_fenced_frame_navigation = false,
      bool is_unfenced_top_navigation = false,
      bool force_new_browsing_instance = false,
      bool is_container_initiated = false,
      bool has_rel_opener = false,
      net::StorageAccessApiStatus storage_access_api_status =
          net::StorageAccessApiStatus::kNone,
      std::optional<std::u16string> embedder_shared_storage_context =
          std::nullopt);

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
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      int initiator_process_id,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener);

  // Used to restart a navigation that was thought to be same-document in
  // cross-document mode.
  void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Cancels the NavigationRequest owned by |frame_tree_node|. Note that this
  // will only cancel NavigationRequests that haven't reached the "pending
  // commit" stage yet, as after that the NavigationRequests will no longer be
  // owned by the FrameTreeNode.
  void CancelNavigation(FrameTreeNode* frame_tree_node,
                        NavigationDiscardReason reason);

  // Called to record the time it took to execute the beforeunload hook for the
  // current navigation. See RenderFrameHostImpl::SendBeforeUnload() for details
  // on `for_legacy`.
  void LogBeforeUnloadTime(base::TimeTicks renderer_before_unload_start_time,
                           base::TimeTicks renderer_before_unload_end_time,
                           base::TimeTicks before_unload_sent_time,
                           bool for_legacy);

  // Called to record the time that the RenderFrameHost told the renderer to
  // commit the current navigation.
  void LogCommitNavigationSent();

  // Returns the NavigationController associated with this Navigator.
  NavigationControllerImpl& controller() { return controller_; }

 private:
  friend class NavigatorTestWithBrowserSideNavigation;

  // Holds data used to track browser side navigation metrics.
  struct NavigationMetricsData;

  void RecordNavigationMetrics(
      const LoadCommittedDetails& details,
      const mojom::DidCommitProvisionalLoadParams& params,
      SiteInstance* site_instance,
      const GURL& original_request_url);

  // Called when a renderer initiated navigation has started. Returns the
  // pending NavigationEntry to be used. Either null or a new one owned
  // NavigationController.
  NavigationEntryImpl* GetNavigationEntryForRendererInitiatedNavigation(
      const blink::mojom::CommonNavigationParams& common_params,
      FrameTreeNode* frame_tree_node,
      bool override_user_agent);

  // Called to record the time it took to execute beforeunload handlers for
  // renderer-inititated navigations. It records the time it took to execute
  // beforeunload handlers in the renderer process before sending the
  // BeginNavigation IPC.
  void LogRendererInitiatedBeforeUnloadTime(
      base::TimeTicks renderer_before_unload_start_time,
      base::TimeTicks renderer_before_unload_end_time);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this Navigator.
  NavigationControllerImpl controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be nullptr in tests.
  raw_ptr<NavigatorDelegate> delegate_;

  // Tracks metrics for each navigation.
  std::unique_ptr<Navigator::NavigationMetricsData> metrics_data_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATOR_H_
