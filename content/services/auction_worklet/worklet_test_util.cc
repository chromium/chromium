// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_test_util.h"

#include <string>

#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"

namespace auction_worklet {

const char kJavascriptMimeType[] = "application/javascript";
const char kJsonMimeType[] = "application/json";

void AddResponse(network::TestURLLoaderFactory* url_loader_factory,
                 const GURL& url,
                 base::Optional<std::string> mime_type,
                 base::Optional<std::string> charset,
                 const std::string content,
                 net::HttpStatusCode http_status,
                 network::TestURLLoaderFactory::Redirects redirects) {
  auto head = network::mojom::URLResponseHead::New();
  std::string status_line(base::StringPrintf(
      "HTTP/1.1 %d %s\r\n\r\n", static_cast<int>(http_status),
      net::GetHttpReasonPhrase(http_status)));
  // Don't bother adding headers, since the script grabs headers from
  // URLResponseHead fields instead of the corresponding
  // net::HttpResponseHeaders fields.
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(status_line));
  if (mime_type)
    head->mime_type = *mime_type;
  if (charset)
    head->charset = *charset;
  url_loader_factory->AddResponse(url, std::move(head), content,
                                  network::URLLoaderCompletionStatus(),
                                  std::move(redirects));
}

void AddJavascriptResponse(network::TestURLLoaderFactory* url_loader_factory,
                           const GURL& url,
                           const std::string content) {
  AddResponse(url_loader_factory, url, kJavascriptMimeType, base::nullopt,
              content);
}

void AddJsonResponse(network::TestURLLoaderFactory* url_loader_factory,
                     const GURL& url,
                     const std::string content) {
  AddResponse(url_loader_factory, url, kJsonMimeType, base::nullopt, content);
}

}  // namespace auction_worklet
