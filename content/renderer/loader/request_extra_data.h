// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
#define CONTENT_CHILD_REQUEST_EXTRA_DATA_H_

#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/renderer/loader/frame_request_blocker.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "ui/base/page_transition_types.h"

namespace network {
struct ResourceRequest;
}

namespace content {

// Can be used by callers to store extra data on every ResourceRequest
// which will be incorporated into the ResourceHostMsg_RequestResource message
// sent by ResourceDispatcher.
class CONTENT_EXPORT RequestExtraData : public blink::WebURLRequest::ExtraData {
 public:
  RequestExtraData();
  ~RequestExtraData() override;

  void set_visibility_state(
      blink::mojom::PageVisibilityState visibility_state) {
    visibility_state_ = visibility_state;
  }
  void set_render_frame_id(int render_frame_id) {
    render_frame_id_ = render_frame_id;
  }
  void set_is_main_frame(bool is_main_frame) {
    is_main_frame_ = is_main_frame;
  }
  void set_allow_download(bool allow_download) {
    allow_download_ = allow_download;
  }
  ui::PageTransition transition_type() const { return transition_type_; }
  void set_transition_type(ui::PageTransition transition_type) {
    transition_type_ = transition_type;
  }
  int service_worker_provider_id() const {
    return service_worker_provider_id_;
  }
  void set_service_worker_provider_id(
      int service_worker_provider_id) {
    service_worker_provider_id_ = service_worker_provider_id;
  }
  // true if the request originated from within a service worker e.g. due to
  // a fetch() in the service worker script.
  void set_originated_from_service_worker(bool originated_from_service_worker) {
    originated_from_service_worker_ = originated_from_service_worker;
  }
  // |custom_user_agent| is used to communicate an overriding custom user agent
  // to |RenderViewImpl::willSendRequest()|; set to a null string to indicate no
  // override and an empty string to indicate that there should be no user
  // agent.
  const blink::WebString& custom_user_agent() const {
    return custom_user_agent_;
  }
  void set_custom_user_agent(const blink::WebString& custom_user_agent) {
    custom_user_agent_ = custom_user_agent;
  }

  // PlzNavigate: |navigation_response_override| is used to override certain
  // parameters of navigation requests.
  std::unique_ptr<NavigationResponseOverrideParameters>
  TakeNavigationResponseOverrideOwnership() {
    return std::move(navigation_response_override_);
  }

  void set_navigation_response_override(
      std::unique_ptr<NavigationResponseOverrideParameters> response_override) {
    navigation_response_override_ = std::move(response_override);
  }

  // |continue_navigation| is used to continue a navigation on the renderer
  // process that has already been started on the browser process.
  base::OnceClosure TakeContinueNavigationFunctionOwnerShip() {
    return std::move(continue_navigation_function_);
  }
  void set_continue_navigation_function(base::OnceClosure continue_navigation) {
    continue_navigation_function_ = std::move(continue_navigation);
  }

  void set_initiated_in_secure_context(bool secure) {
    initiated_in_secure_context_ = secure;
  }

  // The request is for a prefetch-only client (i.e. running NoStatePrefetch)
  // and should use LOAD_PREFETCH network flags.
  bool is_for_no_state_prefetch() const { return is_for_no_state_prefetch_; }
  void set_is_for_no_state_prefetch(bool prefetch) {
    is_for_no_state_prefetch_ = prefetch;
  }

  // Copy of the settings value determining if mixed plugin content should be
  // blocked.
  bool block_mixed_plugin_content() const {
    return block_mixed_plugin_content_;
  }
  void set_block_mixed_plugin_content(bool block_mixed_plugin_content) {
    block_mixed_plugin_content_ = block_mixed_plugin_content;
  }

  // Indicates whether a navigation was initiated by the browser or renderer.
  bool navigation_initiated_by_renderer() const {
    return navigation_initiated_by_renderer_;
  }
  void set_navigation_initiated_by_renderer(bool navigation_by_renderer) {
    navigation_initiated_by_renderer_ = navigation_by_renderer;
  }

  // Determines whether SameSite cookies will be attached to the request
  // even when the request looks cross-site.
  bool attach_same_site_cookies() const { return attach_same_site_cookies_; }
  void set_attach_same_site_cookies(bool attach) {
    attach_same_site_cookies_ = attach;
  }

  std::vector<std::unique_ptr<URLLoaderThrottle>> TakeURLLoaderThrottles() {
    return std::move(url_loader_throttles_);
  }
  void set_url_loader_throttles(
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles) {
    url_loader_throttles_ = std::move(throttles);
  }

  void set_frame_request_blocker(
      scoped_refptr<FrameRequestBlocker> frame_request_blocker) {
    frame_request_blocker_ = frame_request_blocker;
  }

  scoped_refptr<FrameRequestBlocker> frame_request_blocker() {
    return frame_request_blocker_;
  }

  void CopyToResourceRequest(network::ResourceRequest* request) const;

 private:
  blink::mojom::PageVisibilityState visibility_state_;
  int render_frame_id_;
  bool is_main_frame_;
  bool allow_download_;
  ui::PageTransition transition_type_;
  int service_worker_provider_id_;
  bool originated_from_service_worker_;
  blink::WebString custom_user_agent_;
  std::unique_ptr<NavigationResponseOverrideParameters>
      navigation_response_override_;
  // TODO(arthursonzogni): Move most of the |navigation_response_override_|
  // content as parameters of this function.
  base::OnceClosure continue_navigation_function_;
  bool initiated_in_secure_context_;
  bool is_for_no_state_prefetch_;
  bool block_mixed_plugin_content_;
  bool navigation_initiated_by_renderer_;
  bool attach_same_site_cookies_;
  std::vector<std::unique_ptr<URLLoaderThrottle>> url_loader_throttles_;
  scoped_refptr<FrameRequestBlocker> frame_request_blocker_;

  DISALLOW_COPY_AND_ASSIGN(RequestExtraData);
};

}  // namespace content

#endif  // CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
