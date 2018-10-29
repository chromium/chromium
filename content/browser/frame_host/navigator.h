// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/navigation_controller.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;

namespace base {
class TimeTicks;
}

namespace network {
class ResourceRequestBody;
}

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class RenderFrameHostImpl;
struct CommonNavigationParams;

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

  // The RenderFrameHostImpl has failed a provisional load.
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {}

  // The RenderFrameHostImpl has failed to load the document.
  virtual void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                                    const GURL& url,
                                    int error_code,
                                    const base::string16& error_description) {}

  // The RenderFrameHostImpl has committed a navigation. The Navigator is
  // responsible for resetting |navigation_handle| at the end of this method and
  // should not attempt to keep it alive.
  // Note: it is possible that |navigation_handle| is not the NavigationHandle
  // stored in the RenderFrameHost that just committed. This happens for example
  // when a same-page navigation commits while another navigation is ongoing.
  // The Navigator should use the NavigationHandle provided by this method and
  // not attempt to access the RenderFrameHost's NavigationsHandle.
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      std::unique_ptr<NavigationHandleImpl> navigation_handle,
      bool was_within_same_document) {}

  // Called on a newly created subframe during a history navigation. The browser
  // process looks up the corresponding FrameNavigationEntry for the new frame
  // navigates it in the correct process. Returns false if the
  // FrameNavigationEntry can't be found or the navigation fails.
  // TODO(creis): Remove |default_url| once we have collected UMA stats on the
  // cases that we use a different URL from history than the frame's src.
  virtual bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      const GURL& default_url);

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
      bool uses_post,
      const scoped_refptr<network::ResourceRequestBody>& body,
      const std::string& extra_headers,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      bool should_replace_current_entry,
      bool user_gesture,
      blink::WebTriggeringEventInfo triggering_event_info,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {}

  // Called when a document requests a navigation in another document through a
  // RenderFrameProxy. If |method| is "POST", then |post_body| needs to specify
  // the request body, otherwise |post_body| should be null.
  virtual void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {}

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
      const CommonNavigationParams& common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator);

  // Used to restart a navigation that was thought to be same-document in
  // cross-document mode.
  virtual void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) {}

  // Used to abort an ongoing renderer-initiated navigation.
  // Only used with PerNavigationMojoInterface disabled.
  virtual void OnAbortNavigation(FrameTreeNode* frame_tree_node) {}

  // Cancel a NavigationRequest for |frame_tree_node|. If the request is
  // renderer-initiated and |inform_renderer| is true, an IPC will be sent to
  // the renderer process to inform it that the navigation it requested was
  // cancelled.
  virtual void CancelNavigation(FrameTreeNode* frame_tree_node,
                                bool inform_renderer) {}

  // Called when the network stack started handling the navigation request
  // so that the |timestamp| when it happened can be recorded into an histogram.
  // The |url| is used to verify we're tracking the correct navigation.
  // TODO(carlosk): once PlzNavigate is the only navigation implementation
  // remove the URL parameter and rename this method to better suit its naming
  // conventions.
  virtual void LogResourceRequestTime(base::TimeTicks timestamp,
                                      const GURL& url) {}

  // Called to record the time it took to execute the before unload hook for the
  // current navigation.
  virtual void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) {}

  // Called when a navigation has failed or the response is 204/205 to discard
  // the pending entry in order to avoid url spoofs. |expected_pending_entry_id|
  // is the ID of the pending NavigationEntry at the start of the navigation.
  // With sufficiently bad interleaving of IPCs, this may no longer be the
  // pending NavigationEntry, in which case the pending NavigationEntry will not
  // be discarded.
  virtual void DiscardPendingEntryIfNeeded(int expected_pending_entry_id) {}

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
