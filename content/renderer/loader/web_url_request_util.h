// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_
#define CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace blink {
class WebHTTPBody;
}

namespace content {

ResourceType RequestContextToResourceType(
    blink::mojom::RequestContextType request_context);

CONTENT_EXPORT ResourceType WebURLRequestToResourceType(
    const blink::WebURLRequest& request);

net::HttpRequestHeaders GetWebURLRequestHeaders(
    const blink::WebURLRequest& request);

std::string GetWebURLRequestHeadersAsString(
    const blink::WebURLRequest& request);

// Takes a ResourceRequestBody and converts into WebHTTPBody.
blink::WebHTTPBody GetWebHTTPBodyForRequestBody(
    const network::ResourceRequestBody& input);

// Takes a WebHTTPBody and converts into a ResourceRequestBody.
scoped_refptr<network::ResourceRequestBody> GetRequestBodyForWebHTTPBody(
    const blink::WebHTTPBody& httpBody);

// Takes a WebURLRequest and sets the appropriate information
// in a ResourceRequestBody structure. Returns an empty scoped_refptr
// if the request body is not present.
scoped_refptr<network::ResourceRequestBody> GetRequestBodyForWebURLRequest(
    const blink::WebURLRequest& request);

// Helper functions to convert enums from the blink type to the content
// type.
std::string GetFetchIntegrityForWebURLRequest(
    const blink::WebURLRequest& request);
blink::mojom::RequestContextType GetRequestContextTypeForWebURLRequest(
    const blink::WebURLRequest& request);
blink::WebMixedContentContextType GetMixedContentContextTypeForWebURLRequest(
    const blink::WebURLRequest& request);

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_URL_REQUEST_UTIL_H_
