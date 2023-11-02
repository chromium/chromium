// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_

#include <string>
#include <vector>

#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace auction_worklet {

class AuctionV8Helper;

// The official Javascript, JSON, and WASM MIME types. For JS and JSON there are
// also other supported MIME types.
extern const char kJavascriptMimeType[];
extern const char kJsonMimeType[];
extern const char kWasmMimeType[];

// "X-Allow-Fledge: true" header.
extern const char kAllowFledgeHeader[];

// Enqueues a response to `url_loader_factory` using the specified values.
//
// `headers` contains the HTTP header lines (no status line + header lines) used
// to create the HttpResponseHeaders value. If nullopt, HttpResponseHeaders is
// null, and `http_status` is ignored.
void AddResponse(network::TestURLLoaderFactory* url_loader_factory,
                 const GURL& url,
                 absl::optional<std::string> mime_type,
                 absl::optional<std::string> charset,
                 const std::string content,
                 absl::optional<std::string> headers = kAllowFledgeHeader,
                 net::HttpStatusCode http_status = net::HTTP_OK,
                 network::TestURLLoaderFactory::Redirects redirects =
                     network::TestURLLoaderFactory::Redirects());

// Convenience methods to invoke AddResponse() with the specified MIME type and
// no charset.
void AddJavascriptResponse(network::TestURLLoaderFactory* url_loader_factory,
                           const GURL& url,
                           const std::string content);
void AddJsonResponse(network::TestURLLoaderFactory* url_loader_factory,
                     const GURL& url,
                     const std::string content);
void AddVersionedJsonResponse(network::TestURLLoaderFactory* url_loader_factory,
                              const GURL& url,
                              const std::string content,
                              uint32_t data_version);

// Adds a bidder worklet JSON response, optionally with data version and format
// version headers. Defaults to including a header indicating format version 2.
void AddBidderJsonResponse(
    network::TestURLLoaderFactory* url_loader_factory,
    const GURL& url,
    const std::string content,
    absl::optional<uint32_t> data_version = absl::nullopt,
    const absl::optional<std::string>& format_version_string = "2");

// Adds a task to `v8_helper->v8_runner()` that blocks until the return value
// is signaled. The returned event will be deleted afterwards.
base::WaitableEvent* WedgeV8Thread(AuctionV8Helper* v8_helper);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_
