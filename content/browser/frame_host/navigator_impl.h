// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/common/previews_state.h"
#include "url/gurl.h"

class GURL;

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
struct LoadCommittedDetails;

// This class is an implementation of Navigator, responsible for managing
// navigations in regular browser tabs.
class CONTENT_EXPORT NavigatorImpl : public Navigator {
 public:
  NavigatorImpl(NavigationControllerImpl* navigation_controller,
                NavigatorDelegate* delegate);

  static void CheckWebUIRendererDoesNotDisplayNormalURL(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url);

  // Navigator implementation.
  NavigatorDelegate* GetDelegate() override;
  NavigationController* GetController() override;
  void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params)
      override;
  void DidFailLoadWithError(RenderFrameHostImpl* render_frame_host,
                            const GURL& url,
                            int error_code,
                            const base::string16& error_description) override;
  void DidNavigate(RenderFrameHostImpl* render_frame_host,
                   const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   std::unique_ptr<NavigationHandleImpl> navigation_handle,
                   bool was_within_same_document) override;
  bool StartHistoryNavigationInNewSubframe(
      RenderFrameHostImpl* render_frame_host,
      const GURL& default_url) override;
  void Navigate(std::unique_ptr<NavigationRequest> request,
                ReloadType reload_type,
                RestoreType restore_type) override;
  void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                      const GURL& url,
                      bool uses_post,
                      const scoped_refptr<network::ResourceRequestBody>& body,
                      const std::string& extra_headers,
                      const Referrer& referrer,
                      WindowOpenDisposition disposition,
                      bool should_replace_current_entry,
                      bool user_gesture,
                      blink::WebTriggeringEventInfo triggering_event_info,
                      scoped_refptr<network::SharedURLLoaderFactory>
                          blob_url_loader_factory) override;
  void NavigateFromFrameProxy(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      SiteInstance* source_site_instance,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry,
      const std::string& method,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const std::string& extra_headers,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory)
      override;
  void OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                         bool proceed,
                         const base::TimeTicks& proceed_time) override;
  void OnBeginNavigation(
      FrameTreeNode* frame_tree_node,
      const CommonNavigationParams& common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator) override;
  void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) override;
  void OnAbortNavigation(FrameTreeNode* frame_tree_node) override;
  void LogResourceRequestTime(base::TimeTicks timestamp,
                              const GURL& url) override;
  void LogBeforeUnloadTime(
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time) override;
  void CancelNavigation(FrameTreeNode* frame_tree_node,
                        bool inform_renderer) override;
  void DiscardPendingEntryIfNeeded(int expected_pending_entry_id) override;

 private:
  // Holds data used to track browser side navigation metrics.
  struct NavigationMetricsData;

  friend class NavigatorTestWithBrowserSideNavigation;
  ~NavigatorImpl() override;

  void RecordNavigationMetrics(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      SiteInstance* site_instance);

  // Called when a navigation has started in a main frame, to update the pending
  // NavigationEntry if the controller does not currently have a
  // browser-initiated one.
  void DidStartMainFrameNavigation(const GURL& url,
                                   SiteInstanceImpl* site_instance,
                                   NavigationHandleImpl* navigation_handle);

  // The NavigationController that will keep track of session history for all
  // RenderFrameHost objects using this NavigatorImpl.
  // TODO(nasko): Move ownership of the NavigationController from
  // WebContentsImpl to this class.
  NavigationControllerImpl* controller_;

  // Used to notify the object embedding this Navigator about navigation
  // events. Can be NULL in tests.
  NavigatorDelegate* delegate_;

  std::unique_ptr<NavigatorImpl::NavigationMetricsData> navigation_data_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_IMPL_H_
