// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/ukm_source_id.h"
#include "base/optional.h"
#include "base/values.h"
#include "content/public/browser/global_routing_id.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "ipc/ipc_message.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

class ExtensionNavigationUIData;

// Helper struct to initialize WebRequestInfo.
struct WebRequestInfoInitParams {
  WebRequestInfoInitParams();
  WebRequestInfoInitParams(WebRequestInfoInitParams&& other);
  WebRequestInfoInitParams& operator=(WebRequestInfoInitParams&& other);

  // Initializes a WebRequestInfoInitParams from information provided over a
  // URLLoaderFactory interface.
  WebRequestInfoInitParams(
      uint64_t request_id,
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
      int32_t routing_id,
      const network::ResourceRequest& request,
      bool is_download,
      bool is_async,
      bool is_service_worker_script,
      base::Optional<int64_t> navigation_id,
      base::UkmSourceId ukm_source_id);

  ~WebRequestInfoInitParams();

  uint64_t id = 0;
  GURL url;
  int render_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  int frame_id = -1;
  std::string method;
  bool is_navigation_request = false;
  base::Optional<url::Origin> initiator;
  blink::mojom::ResourceType type = blink::mojom::ResourceType::kSubResource;
  WebRequestResourceType web_request_type = WebRequestResourceType::OTHER;
  bool is_async = false;
  net::HttpRequestHeaders extra_request_headers;
  std::unique_ptr<base::DictionaryValue> request_body_data;
  bool is_web_view = false;
  int web_view_instance_id = -1;
  int web_view_rules_registry_id = -1;
  int web_view_embedder_process_id = -1;
  ExtensionApiFrameIdMap::FrameData frame_data;
  bool is_service_worker_script = false;
  base::Optional<int64_t> navigation_id;
  base::UkmSourceId ukm_source_id = base::kInvalidUkmSourceId;
  content::GlobalFrameRoutingId parent_routing_id;

 private:
  void InitializeWebViewAndFrameData(
      const ExtensionNavigationUIData* navigation_ui_data);

  DISALLOW_COPY_AND_ASSIGN(WebRequestInfoInitParams);
};

// A URL request representation used by WebRequest API internals. This structure
// carries information about an in-progress request.
struct WebRequestInfo {
  explicit WebRequestInfo(WebRequestInfoInitParams params);

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

  // The routing ID of the object which initiated the request, if applicable.
  const int routing_id = MSG_ROUTING_NONE;

  // The render frame ID of the frame which initiated this request, or -1 if
  // the request was not initiated by a frame.
  const int frame_id;

  // The HTTP method used for the request, if applicable.
  const std::string method;

  // Indicates whether the request is for a browser-side navigation.
  const bool is_navigation_request;

  // The origin of the context which initiated the request. May be null for
  // browser-initiated requests such as navigations.
  const base::Optional<url::Origin> initiator;

  // Extension API frame data corresponding to details of the frame which
  // initiate this request.
  ExtensionApiFrameIdMap::FrameData frame_data;

  // The type of the request (e.g. main frame, subresource, XHR, etc).
  const blink::mojom::ResourceType type;

  // A partially mirrored copy of |type| which is slightly less granular and
  // which also identifies WebSocket requests separately from other types.
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
  std::unique_ptr<base::DictionaryValue> request_body_data;

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
  mutable base::Optional<std::vector<declarative_net_request::RequestAction>>
      dnr_actions;

  const bool is_service_worker_script;

  // Valid if this request corresponds to a navigation.
  const base::Optional<int64_t> navigation_id;

  // UKM source to associate metrics with for this request.
  const base::UkmSourceId ukm_source_id;

  // ID of the RenderFrameHost corresponding to the parent frame. Only valid for
  // document subresource and sub-frame requests.
  const content::GlobalFrameRoutingId parent_routing_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestInfo);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_INFO_H_
