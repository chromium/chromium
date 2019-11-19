// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
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

class BundledExchangesHandleTracker;
class FrameNavigationEntry;
class FrameTreeNode;
class PrefetchedSignedExchangeCache;
class RenderFrameHostImpl;

// Implementations of this interface are responsible for performing navigations
// in a node of the FrameTree. Its lifetime is bound to all FrameTreeNode
// objects that are using it and will be released once all nodes that use it are
// freed. The Navigator is bound to a single frame tree and cannot be used by
// multiple instances of FrameTree.
// TODO(nasko): Move all navigation methods, such as didStartProvisionalLoad
// from WebContentsImpl to this interface.
class CONTENT_EXPORT Navigator : public base::RefCounted<Navigator> {
 public:
  // Returns the delegate of this Navigator.
  virtual NavigatorDelegate* GetDelegate();

  // Returns the NavigationController associated with this Navigator.
  virtual NavigationController* GetController();

  // Notifications coming from the RenderFrameHosts ----------------------------

  // The RenderFrameHostImpl has failed to load the document.
  virtual void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                                    const GURL& url,
                                    int error_code,
                                    const base::string16& error_description) {}

  // The RenderFrameHostImpl has committed a navigation. The Navigator is
  // responsible for resetting |navigation_request| at the end of this method
  // and should not attempt to keep it alive. Note: it is possible that
  // |navigation_request| is not the NavigationRequest stored in the
  // RenderFrameHost that just committed. This happens for example when a
  // same-page navigation commits while another navigation is ongoing. The
  // Navigator should use the NavigationRequest provided by this method and not
  // attempt to access the RenderFrameHost's NavigationsRequests.
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      std::unique_ptr<NavigationRequest> navigation_request,
      bool was_within_same_document) {}

  // Called on a newly created subframe during a history navigation. The browser
  // process looks up the corresponding FrameNavigationEntry for the new frame
  // navigates it in the correct process. Returns false if the
  // FrameNavigationEntry can't be found or the navigation fails.
  virtual bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>*
          navigation_client);

  // Navigation requests -------------------------------------------------------

  // Called by the NavigationController to cause the Navigator to navigate to
  // |navigation_request|. The NavigationController should be called back with
  // RendererDidNavigate on success or DiscardPendingEntry on failure. The
  // callbacks should be called in a future iteration of the message loop.
  virtual void Navigate(std::unique_ptr<NavigationRequest> request,
                        ReloadType reload_type,
                        RestoreType restore_type) {}

  virtual base::TimeTicks GetCurrentLoadStart();

  // The RenderFrameHostImpl has received a request to open a URL with the
  // specified |disposition|.
  virtual void RequestOpenURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      const base::Optional<url::Origin>& initiator_origin,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture,
      blink::TriggeringEventInfo triggering_event_info,
      const std::string& href_translate,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {}

  // Called when a document requests a navigation in another document through a
  // RenderFrameProxy. If |method| is "POST", then |post_body| needs to specify
  // the request body, otherwise |post_body| should be null.
  virtual void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
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
      bool has_user_gesture) {}

  // Called after receiving a BeforeUnloadACK IPC from the renderer. If
  // |frame_tree_node| has a NavigationRequest waiting for the renderer
  // response, then the request is either started or canceled, depending on the
  // value of |proceed|.
  virtual void OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                                 bool proceed,
                                 const base::TimeTicks& proceed_time) {}

  // Used to start a new renderer-initiated navigation, following a
  // BeginNavigation IPC from the renderer.
  virtual void OnBeginNavigation(
      FrameTreeNode* frame_tree_node,
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      std::unique_ptr<BundledExchangesHandleTracker>
          bundled_exchanges_handle_tracker);

  // Used to restart a navigation that was thought to be same-document in
  // cross-document mode.
  virtual void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) {}

  // Cancel a NavigationRequest for |frame_tree_node|.
  virtual void CancelNavigation(FrameTreeNode* frame_tree_node) {}

  // Called when the network stack started handling the navigation request
  // so that the |timestamp| when it happened can be recorded into an histogram.
  // The |url| is used to verify we're tracking the correct navigation.
  // TODO(carlosk): Remove the URL parameter and rename this method to better
  // suit naming conventions.
  virtual void LogResourceRequestTime(base::TimeTicks timestamp,
                                      const GURL& url) {}

  // Called to record the time it took to execute the before unload hook for the
  // current navigation.
  virtual void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) {}

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
