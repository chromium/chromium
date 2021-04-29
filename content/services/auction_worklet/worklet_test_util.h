// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace auction_worklet {

// The official Javascript and JSON MIME types. These are not the only supported
// MIME types for either, however.
extern const char kJavascriptMimeType[];
extern const char kJsonMimeType[];

// "X-Allow-Fledge: true" header.
extern const char kAllowFledgeHeader[];

// Enqueues a response to `url_loader_factory` using the specified values.
//
// `headers` contains the HTTP header lines (no status line + header lines) used
// to create the HttpResponseHeaders value. If nullopt, HttpResponseHeaders is
// null, and `http_status` is ignored.
void AddResponse(network::TestURLLoaderFactory* url_loader_factory,
                 const GURL& url,
                 base::Optional<std::string> mime_type,
                 base::Optional<std::string> charset,
                 const std::string content,
                 base::Optional<std::string> headers = kAllowFledgeHeader,
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

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_TEST_UTIL_H_
