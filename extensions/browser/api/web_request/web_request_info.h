// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "content/public/browser/global_routing_id.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

class ExtensionNavigationUIData;

// Helper struct to initialize WebRequestInfo.
struct WebRequestInfoInitParams {
  WebRequestInfoInitParams();

  // Initializes a WebRequestInfoInitParams from information provided over a
  // URLLoaderFactory interface.
  WebRequestInfoInitParams(
      uint64_t request_id,
      int render_process_id,
      int frame_routing_id,
      std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
      const network::ResourceRequest& request,
      bool is_download,
      bool is_async,
      bool is_service_worker_script,
      absl::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id);

  WebRequestInfoInitParams(const WebRequestInfoInitParams&) = delete;
  WebRequestInfoInitParams(WebRequestInfoInitParams&& other);

  WebRequestInfoInitParams& operator=(const WebRequestInfoInitParams&) = delete;
  WebRequestInfoInitParams& operator=(WebRequestInfoInitParams&& other);

  ~WebRequestInfoInitParams();

  uint64_t id = 0;
  GURL url;
  int render_process_id = -1;
  int frame_routing_id = MSG_ROUTING_NONE;
  std::string method;
  bool is_navigation_request = false;
  absl::optional<url::Origin> initiator;
  WebRequestResourceType web_request_type = WebRequestResourceType::OTHER;
  bool is_async = false;
  net::HttpRequestHeaders extra_request_headers;
  absl::optional<base::Value::Dict> request_body_data;
  bool is_web_view = false;
  int web_view_instance_id = -1;
  int web_view_rules_registry_id = -1;
  int web_view_embedder_process_id = -1;
  ExtensionApiFrameIdMap::FrameData frame_data;
  bool is_service_worker_script = false;
  absl::optional<int64_t> navigation_id;
  ukm::SourceIdObj ukm_source_id = ukm::kInvalidSourceIdObj;
  content::GlobalRenderFrameHostId parent_routing_id;

 private:
  void InitializeWebViewAndFrameData(
      const ExtensionNavigationUIData* navigation_ui_data);
};

// A URL request representation used by WebRequest API internals. This structure
// carries information about an in-progress request.
struct WebRequestInfo {
  explicit WebRequestInfo(WebRequestInfoInitParams params);

  WebRequestInfo(const WebRequestInfo&) = delete;
  WebRequestInfo& operator=(const WebRequestInfo&) = delete;

  ~WebRequestInfo();

  // Fill in response data for this request.
  void AddResponseInfoFromResourceResponse(
      const network::mojom::URLResponseHead& response);

  // A unique identifier for this request.
  const uint64_t id;

  // The URL of the request.
  const GURL url;

  // The ID of the render process which initiated the request, or -1 of not
  // applicable (i.e. if initiated by the browser).
  const int render_process_id;

  // The frame routing ID of the frame which initiated this request, or
  // MSG_ROUTING_NONE if the request was not initiated by a frame.
  const int frame_routing_id = MSG_ROUTING_NONE;

  // The HTTP method used for the request, if applicable.
  const std::string method;

  // Indicates whether the request is for a browser-side navigation.
  const bool is_navigation_request;

  // The origin of the context which initiated the request. May be null for
  // browser-initiated requests such as navigations.
  const absl::optional<url::Origin> initiator;

  // Extension API frame data corresponding to details of the frame which
  // initiate this request.
  ExtensionApiFrameIdMap::FrameData frame_data;

  // The resource type being requested.
  const WebRequestResourceType web_request_type = WebRequestResourceType::OTHER;

  // Indicates if this request is asynchronous.
  const bool is_async;

  const net::HttpRequestHeaders extra_request_headers;

  // HTTP response code for this request if applicable. -1 if not.
  int response_code = -1;

  // All response headers for this request, if set.
  scoped_refptr<net::HttpResponseHeaders> response_headers;

  // The stringified IP address of the host which provided the response to this
  // request, if applicable and available.
  std::string response_ip;

  // Indicates whether or not the request response (if applicable) came from
  // cache.
  bool response_from_cache = false;

  // A dictionary of request body data matching the format expected by
  // WebRequest API consumers. This may have a "formData" key and/or a "raw"
  // key. See WebRequest API documentation for more details.
  absl::optional<base::Value::Dict> request_body_data;

  // Indicates whether this request was initiated by a <webview> instance.
  const bool is_web_view;

  // If |is_web_view| is true, the instance ID, rules registry ID, and embedder
  // process ID pertaining to the webview instance. Note that for browser-side
  // navigation requests, |web_view_embedder_process_id| is always -1.
  const int web_view_instance_id;
  const int web_view_rules_registry_id;
  const int web_view_embedder_process_id;

  // The Declarative Net Request actions associated with this request. Mutable
  // since this is lazily computed. Cached to avoid redundant computations.
  // Valid when not null. In case no actions are taken, populated with an empty
  // vector.
  mutable absl::optional<std::vector<declarative_net_request::RequestAction>>
      dnr_actions;

  const bool is_service_worker_script;

  // Valid if this request corresponds to a navigation.
  const absl::optional<int64_t> navigation_id;

  // UKM source to associate metrics with for this request.
  const ukm::SourceIdObj ukm_source_id;

  // ID of the RenderFrameHost corresponding to the parent frame. Only valid for
  // document subresource and sub-frame requests.
  // TODO(karandeepb, mcnee): For subresources, having "parent" in the name is
  // misleading. This should be renamed to indicate that this is the initiator.
  const content::GlobalRenderFrameHostId parent_routing_id;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
