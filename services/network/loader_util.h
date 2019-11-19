// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LOADER_UTIL_H_
#define SERVICES_NETWORK_LOADER_UTIL_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace net {
class HttpRawRequestHeaders;
class HttpRequestHeaders;
class HttpResponseHeaders;
class URLRequest;
}  // namespace net

// This header contains utility functions and constants shared between network
// service and the old loading path in content/. Once Network Service is the
// only path this should move out of the public directory.
namespace network {
struct HttpRawRequestResponseInfo;
struct ResourceResponse;

// Enumeration for UMA histograms logged by LogConcerningRequestHeaders().
// Entries should not be renumbered and numeric values should never be reused.
// Please keep in sync with "NetworkServiceConcerningRequestHeaders" in
// src/tools/metrics/histograms/enums.xml.
enum class ConcerningHeaderId {
  kConnection = 0,
  kCookie = 1,
  kCookie2 = 2,
  kContentTransferEncoding = 3,
  kDate = 4,
  kExpect = 5,
  kKeepAlive = 6,
  kReferer = 7,
  kTe = 8,
  kTransferEncoding = 9,
  kVia = 10,
  kMaxValue = kVia,
};

// The name of the "Accept" header.
COMPONENT_EXPORT(NETWORK_SERVICE) extern const char kAcceptHeader[];

// Accept header used for frame requests.
COMPONENT_EXPORT(NETWORK_SERVICE)
extern const char kFrameAcceptHeader[];

// The default Accept header value to use if none were specified.
COMPONENT_EXPORT(NETWORK_SERVICE)
extern const char kDefaultAcceptHeader[];

// Helper utilities shared between network service and ResourceDispatcherHost
// code paths.

// Whether the response body should be sniffed in order to determine the MIME
// type of the response.
COMPONENT_EXPORT(NETWORK_SERVICE)
bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response);

// Fill HttpRawRequestResponseInfo based on raw headers.
COMPONENT_EXPORT(NETWORK_SERVICE)
scoped_refptr<HttpRawRequestResponseInfo> BuildRawRequestResponseInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers,
    const net::HttpResponseHeaders* raw_response_headers);

// Returns the referrer based on the validity of the URL and command line flags.
COMPONENT_EXPORT(NETWORK_SERVICE)
std::string ComputeReferrer(const GURL& referrer);

COMPONENT_EXPORT(NETWORK_SERVICE)
void LogConcerningRequestHeaders(const net::HttpRequestHeaders& request_headers,
                                 bool added_during_redirect);

}  // namespace network

#endif  // SERVICES_NETWORK_LOADER_UTIL_H_
