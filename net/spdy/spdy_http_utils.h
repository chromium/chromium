// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_UTILS_H_
#define NET_SPDY_SPDY_HTTP_UTILS_H_

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "url/gurl.h"

namespace net {

class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HttpResponseHeaders;

// HTTP Extensible Priorities header (in lowercase HTTP2/3).
// RFC 9218.
NET_EXPORT extern const char* const kHttp2PriorityHeader;

// Convert a quiche::HttpHeaderBlock into an HttpResponseInfo with some checks.
// `headers` input parameter with the quiche::HttpHeaderBlock.
// `response` output parameter for the HttpResponseInfo.
// Returns OK if successfully converted.  An error is returned if the
// quiche::HttpHeaderBlock is incomplete (e.g. missing 'status' or 'version') or
// checks fail.
NET_EXPORT int SpdyHeadersToHttpResponse(const quiche::HttpHeaderBlock& headers,
                                         HttpResponseInfo* response);

// Converts a quiche::HttpHeaderBlock object into an HttpResponseHeaders object
// by creating a string with embedded nul bytes instead of newlines and then
// parsing it to the HttpResponseHeaders constructor to be parsed. Exposed for
// testing.
// TODO(crbug.com/40282642): Remove this once it is no longer needed.
NET_EXPORT_PRIVATE base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingRawString(
    const quiche::HttpHeaderBlock& headers);

// Converts a quiche::HttpHeaderBlock object into an HttpResponseHeaders object
// by using the HttpResponseHeaders::Builder API. Exposed for testing.
// TODO(crbug.com/40282642): Merge this back into
// SpdyHeadersToHttpResponse() when
// SpdyHeadersToHttpResponseHeadersUsingRawString() is removed.
NET_EXPORT_PRIVATE base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingBuilder(
    const quiche::HttpHeaderBlock& headers);

// Create a quiche::HttpHeaderBlock from HttpRequestInfo and
// HttpRequestHeaders.
NET_EXPORT void CreateSpdyHeadersFromHttpRequest(
    const HttpRequestInfo& info,
    std::optional<RequestPriority> priority,
    const HttpRequestHeaders& request_headers,
    quiche::HttpHeaderBlock* headers);

// Create a quiche::HttpHeaderBlock from HttpRequestInfo and
// HttpRequestHeaders, with the given protocol for extended CONNECT.
// The request's method must be `CONNECT`.
NET_EXPORT void CreateSpdyHeadersFromHttpRequestForExtendedConnect(
    const HttpRequestInfo& info,
    std::optional<RequestPriority> priority,
    const std::string& ext_connect_protocol,
    const HttpRequestHeaders& request_headers,
    quiche::HttpHeaderBlock* headers);

// Create a quiche::HttpHeaderBlock from HttpRequestInfo and HttpRequestHeaders
// for a WebSockets over HTTP/2 request.
NET_EXPORT void CreateSpdyHeadersFromHttpRequestForWebSocket(
    const GURL& url,
    const HttpRequestHeaders& request_headers,
    quiche::HttpHeaderBlock* headers);

// Create HttpRequestHeaders from quiche::HttpHeaderBlock.
NET_EXPORT void ConvertHeaderBlockToHttpRequestHeaders(
    const quiche::HttpHeaderBlock& spdy_headers,
    HttpRequestHeaders* http_headers);

NET_EXPORT spdy::SpdyPriority ConvertRequestPriorityToSpdyPriority(
    RequestPriority priority);

NET_EXPORT RequestPriority
ConvertSpdyPriorityToRequestPriority(spdy::SpdyPriority priority);

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_UTILS_H_
