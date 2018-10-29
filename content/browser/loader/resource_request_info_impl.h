// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/supports_user_data.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_states.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {
class DetachableResourceHandler;
class ResourceContext;
struct GlobalRequestID;
struct GlobalRoutingID;

// Holds the data ResourceDispatcherHost associates with each request.
// Retrieve this data by calling ResourceDispatcherHost::InfoForRequest.
class ResourceRequestInfoImpl : public ResourceRequestInfo,
                                public base::SupportsUserData::Data {
 public:
  // Returns the ResourceRequestInfoImpl associated with the given URLRequest.
  CONTENT_EXPORT static ResourceRequestInfoImpl* ForRequest(
      net::URLRequest* request);

  // And, a const version for cases where you only need read access.
  CONTENT_EXPORT static const ResourceRequestInfoImpl* ForRequest(
      const net::URLRequest* request);

  CONTENT_EXPORT ResourceRequestInfoImpl(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      int route_id,
      int frame_tree_node_id,
      int plugin_child_id,
      int request_id,
      int render_frame_id,
      bool is_main_frame,
      ResourceType resource_type,
      ui::PageTransition transition_type,
      bool is_download,
      bool is_stream,
      bool allow_download,
      bool has_user_gesture,
      bool enable_load_timing,
      bool enable_upload_progress,
      bool do_not_prompt_for_login,
      bool keepalive,
      network::mojom::ReferrerPolicy referrer_policy,
      bool is_prerendering,
      ResourceContext* context,
      bool report_raw_headers,
      bool report_security_info,
      bool is_async,
      PreviewsState previews_state,
      const scoped_refptr<network::ResourceRequestBody> body,
      bool initiated_in_secure_context);
  ~ResourceRequestInfoImpl() override;

  // ResourceRequestInfo implementation:
  WebContentsGetter GetWebContentsGetterForRequest() const override;
  FrameTreeNodeIdGetter GetFrameTreeNodeIdGetterForRequest() const override;
  ResourceContext* GetContext() const override;
  int GetChildID() const override;
  int GetRouteID() const override;
  GlobalRequestID GetGlobalRequestID() const override;
  int GetPluginChildID() const override;
  int GetRenderFrameID() const override;
  int GetFrameTreeNodeId() const override;
  bool IsMainFrame() const override;
  ResourceType GetResourceType() const override;
  int GetProcessType() const override;
  network::mojom::ReferrerPolicy GetReferrerPolicy() const override;
  bool IsPrerendering() const override;
  ui::PageTransition GetPageTransition() const override;
  bool HasUserGesture() const override;
  bool GetAssociatedRenderFrame(int* render_process_id,
                                int* render_frame_id) const override;
  bool IsAsync() const override;
  bool IsDownload() const override;
  // Returns a bitmask of potentially several Previews optimizations.
  PreviewsState GetPreviewsState() const override;
  NavigationUIData* GetNavigationUIData() const override;
  void SetResourceRequestBlockedReason(
      blink::ResourceRequestBlockedReason reason) override;
  base::Optional<blink::ResourceRequestBlockedReason>
  GetResourceRequestBlockedReason() const override;
  base::StringPiece GetCustomCancelReason() const override;

  CONTENT_EXPORT void AssociateWithRequest(net::URLRequest* request);

  CONTENT_EXPORT int GetRequestID() const;
  GlobalRoutingID GetGlobalRoutingID() const;

  // Returns true if raw response headers (including sensitive data such as
  // cookies) should be included with the response.
  bool ShouldReportRawHeaders() const;

  // Returns true if security details (SSL/TLS connection parameters and
  // certificate chain) should be included with the response.
  bool ShouldReportSecurityInfo() const;

  // PlzNavigate
  // The id of the FrameTreeNode that initiated this request (for a navigation
  // request).
  int frame_tree_node_id() const { return frame_tree_node_id_; }

  ResourceRequesterInfo* requester_info() const {
    return requester_info_.get();
  }

  // DetachableResourceHandler for this request.  May be NULL.
  DetachableResourceHandler* detachable_handler() const {
    return detachable_handler_;
  }
  void set_detachable_handler(DetachableResourceHandler* h) {
    detachable_handler_ = h;
  }
  bool keepalive() const { return keepalive_; }

  // Downloads are allowed only as a top level request.
  bool allow_download() const { return allow_download_; }

  // Whether this is a download.
  void set_is_download(bool download) { is_download_ = download; }

  // Whether this is a stream.
  bool is_stream() const { return is_stream_; }
  void set_is_stream(bool stream) { is_stream_ = stream; }

  // Whether this request has been counted towards the number of in flight
  // requests, which is only true for requests that require a file descriptor
  // for their shared memory buffer.
  bool counted_as_in_flight_request() const {
    return counted_as_in_flight_request_;
  }
  void set_counted_as_in_flight_request(bool was_counted) {
    counted_as_in_flight_request_ = was_counted;
  }

  // The approximate in-memory size (bytes) that we credited this request
  // as consuming in |outstanding_requests_memory_cost_map_|.
  int memory_cost() const { return memory_cost_; }
  void set_memory_cost(int cost) { memory_cost_ = cost; }

  bool is_load_timing_enabled() const { return enable_load_timing_; }

  bool is_upload_progress_enabled() const { return enable_upload_progress_; }

  bool do_not_prompt_for_login() const { return do_not_prompt_for_login_; }
  void set_do_not_prompt_for_login(bool do_not_prompt) {
    do_not_prompt_for_login_ = do_not_prompt;
  }

  const scoped_refptr<network::ResourceRequestBody>& body() const {
    return body_;
  }
  void ResetBody();

  bool initiated_in_secure_context() const {
    return initiated_in_secure_context_;
  }
  void set_initiated_in_secure_context_for_testing(bool secure) {
    initiated_in_secure_context_ = secure;
  }

  void set_navigation_ui_data(
      std::unique_ptr<NavigationUIData> navigation_ui_data) {
    navigation_ui_data_ = std::move(navigation_ui_data);
  }

  void SetBlobHandles(BlobHandles blob_handles);

  bool blocked_response_from_reaching_renderer() const {
    return blocked_response_from_reaching_renderer_;
  }
  void set_blocked_response_from_reaching_renderer(bool value) {
    blocked_response_from_reaching_renderer_ = value;
  }
  bool should_report_corb_blocking() const {
    return should_report_corb_blocking_;
  }
  void set_should_report_corb_blocking(bool value) {
    should_report_corb_blocking_ = value;
  }

  void set_custom_cancel_reason(base::StringPiece reason) {
    custom_cancel_reason_ = reason.as_string();
  }

  bool first_auth_attempt() const { return first_auth_attempt_; }

  void set_first_auth_attempt(bool first_auth_attempt) {
    first_auth_attempt_ = first_auth_attempt;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           DeletedFilterDetached);
  FRIEND_TEST_ALL_PREFIXES(ResourceDispatcherHostTest,
                           DeletedFilterDetachedRedirect);
  // Non-owning, may be NULL.
  DetachableResourceHandler* detachable_handler_;

  scoped_refptr<ResourceRequesterInfo> requester_info_;
  int route_id_;
  const int frame_tree_node_id_;
  int plugin_child_id_;
  int request_id_;
  int render_frame_id_;
  bool is_main_frame_;
  bool is_download_;
  bool is_stream_;
  bool allow_download_;
  bool has_user_gesture_;
  bool enable_load_timing_;
  bool enable_upload_progress_;
  bool do_not_prompt_for_login_;
  bool keepalive_;
  bool counted_as_in_flight_request_;
  ResourceType resource_type_;
  ui::PageTransition transition_type_;
  int memory_cost_;
  network::mojom::ReferrerPolicy referrer_policy_;
  bool is_prerendering_;
  ResourceContext* context_;
  bool report_raw_headers_;
  bool report_security_info_;
  bool is_async_;
  base::Optional<blink::ResourceRequestBlockedReason>
      resource_request_blocked_reason_;
  PreviewsState previews_state_;
  scoped_refptr<network::ResourceRequestBody> body_;
  bool initiated_in_secure_context_;
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // Whether response details (response headers, timing information, metadata)
  // have been blocked from reaching the renderer process (e.g. by Cross-Origin
  // Read Blocking).
  bool blocked_response_from_reaching_renderer_;

  bool should_report_corb_blocking_;
  bool first_auth_attempt_;

  // Keeps upload body blobs alive for the duration of the request.
  BlobHandles blob_handles_;

  std::string custom_cancel_reason_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestInfoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_REQUEST_INFO_IMPL_H_
