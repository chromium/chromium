// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_utils.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/http_raw_headers.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_WIN)
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

mojom::URLResponseHeadPtr CreateURLResponseHead(
    net::HttpStatusCode http_status) {
  auto head = mojom::URLResponseHead::New();
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", static_cast<int>(http_status),
                         net::GetHttpReasonPhrase(http_status)));
  std::string headers = status_line + "\nContent-type: text/html\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  return head;
}

void AddCookiesToURLResponseHead(const std::vector<std::string>& cookies,
                                 mojom::URLResponseHead* response_head) {
  for (const auto& cookie_string : cookies) {
    response_head->headers->AddCookie(cookie_string);
  }
}

mojom::NetworkContextParamsPtr CreateNetworkContextParamsForTesting() {
  mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
  params->file_paths = mojom::NetworkContextFilePaths::New();
#if BUILDFLAG(IS_WIN) && DCHECK_IS_ON()
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
