// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_utils.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if defined(OS_WIN)
#include "base/dcheck_is_on.h"
#endif

namespace network {

std::string GetUploadData(const network::ResourceRequest& request) {
  auto body = request.request_body;
  if (!body || body->elements()->empty())
    return std::string();

  CHECK_EQ(1u, body->elements()->size());
  const auto& element = body->elements()->at(0);
  CHECK_EQ(mojom::DataElementDataView::Tag::kBytes, element.type());
  return std::string(element.As<DataElementBytes>().AsStringPiece());
}

mojom::URLResponseHeadPtr CreateURLResponseHead(net::HttpStatusCode http_status,
                                                bool report_raw_headers) {
  auto head = mojom::URLResponseHead::New();
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", static_cast<int>(http_status),
                         net::GetHttpReasonPhrase(http_status)));
  std::string headers = status_line + "\nContent-type: text/html\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  if (report_raw_headers) {
    head->raw_request_response_info = mojom::HttpRawRequestResponseInfo::New();
    head->raw_request_response_info->http_status_text = status_line;
    head->raw_request_response_info->http_status_code = http_status;
  }
  return head;
}

void AddCookiesToURLResponseHead(const std::vector<std::string>& cookies,
                                 mojom::URLResponseHead* response_head) {
  for (const auto& cookie_string : cookies) {
    response_head->headers->AddCookie(cookie_string);
    if (response_head->raw_request_response_info) {
      response_head->raw_request_response_info->response_headers.push_back(
          mojom::HttpRawHeaderPair::New("Set-Cookie", cookie_string));
    }
  }
}

mojom::NetworkContextParamsPtr CreateNetworkContextParamsForTesting() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
#if defined(OS_WIN) && DCHECK_IS_ON()
  // For unit tests, no need to verify that permissions on the files are
  // correct, as this testing is done in integration tests.
  params->win_permissions_set = true;
#endif
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  params->initial_proxy_config = net::ProxyConfigWithAnnotation::CreateDirect();
  return params;
}

}  // namespace network
