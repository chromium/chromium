// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_test_util.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace auction_worklet {

const char kJavascriptMimeType[] = "application/javascript";
const char kJsonMimeType[] = "application/json";
const char kWasmMimeType[] = "application/wasm";

const char kAllowFledgeHeader[] = "X-Allow-FLEDGE: true";

void AddResponse(network::TestURLLoaderFactory* url_loader_factory,
                 const GURL& url,
                 absl::optional<std::string> mime_type,
                 absl::optional<std::string> charset,
                 const std::string content,
                 absl::optional<std::string> headers,
                 net::HttpStatusCode http_status,
                 network::TestURLLoaderFactory::Redirects redirects) {
  auto head = network::mojom::URLResponseHead::New();
  if (mime_type)
    head->mime_type = *mime_type;
  if (charset)
    head->charset = *charset;

  std::string full_headers =
      base::StringPrintf("HTTP/1.1 %d %s\r\n\r\n",
                         static_cast<int>(http_status),
                         net::GetHttpReasonPhrase(http_status)) +
      headers.value_or(std::string());
  head->headers = net::HttpResponseHeaders::TryToCreate(full_headers);
  CHECK(head->headers);

  // WASM handling cares about content-type header, so add one if needed.
  // This doesn't try to escape, and purposefully passes any weird stuff passed
  // in for easier checking of it.
  if (mime_type) {
    std::string content_type_str = *mime_type;
    if (charset)
      base::StrAppend(&content_type_str, {";charset=", *charset});
    head->headers->SetHeader(net::HttpRequestHeaders::kContentType,
                             content_type_str);
  }

  url_loader_factory->AddResponse(url, std::move(head), content,
                                  network::URLLoaderCompletionStatus(),
                                  std::move(redirects));
}

void AddJavascriptResponse(network::TestURLLoaderFactory* url_loader_factory,
                           const GURL& url,
                           const std::string content) {
  AddResponse(url_loader_factory, url, kJavascriptMimeType, absl::nullopt,
              content);
}

void AddJsonResponse(network::TestURLLoaderFactory* url_loader_factory,
                     const GURL& url,
                     const std::string content) {
  AddResponse(url_loader_factory, url, kJsonMimeType, absl::nullopt, content);
}

void AddVersionedJsonResponse(network::TestURLLoaderFactory* url_loader_factory,
                              const GURL& url,
                              const std::string content,
                              uint32_t data_version) {
  std::string headers = base::StringPrintf("%s\nData-Version: %u",
                                           kAllowFledgeHeader, data_version);
  AddResponse(url_loader_factory, url, kJsonMimeType, absl::nullopt, content,
              headers);
}

void AddBidderJsonResponse(
    network::TestURLLoaderFactory* url_loader_factory,
    const GURL& url,
    const std::string content,
    absl::optional<uint32_t> data_version,
    const absl::optional<std::string>& format_version_string) {
  std::string headers = kAllowFledgeHeader;
  if (data_version)
    headers.append(base::StringPrintf("\nData-Version: %u", *data_version));
  if (format_version_string) {
    headers.append(
        base::StringPrintf("\nX-Fledge-Bidding-Signals-Format-Version:  %s",
                           format_version_string->c_str()));
  }
  AddResponse(url_loader_factory, url, kJsonMimeType, absl::nullopt, content,
              headers);
}

base::WaitableEvent* WedgeV8Thread(AuctionV8Helper* v8_helper) {
  auto event = std::make_unique<base::WaitableEvent>();
  base::WaitableEvent* event_handle = event.get();
  v8_helper->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<base::WaitableEvent> event) { event->Wait(); },
          std::move(event)));
  return event_handle;
}

}  // namespace auction_worklet
