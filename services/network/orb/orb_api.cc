// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/orb/orb_api.h"

#include <string>
#include <unordered_set>

#include "net/http/http_response_headers.h"
#include "services/network/orb/orb_impl.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network::orb {

namespace {

void RemoveAllHttpResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  std::unordered_set<std::string> names_of_headers_to_remove;

  size_t it = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&it, &name, &value))
    names_of_headers_to_remove.insert(base::ToLowerASCII(name));

  headers->RemoveHeaders(names_of_headers_to_remove);
}

}  // namespace

ResponseAnalyzer::~ResponseAnalyzer() = default;

// static
std::unique_ptr<ResponseAnalyzer> ResponseAnalyzer::Create(
    PerFactoryState* state) {
  return std::make_unique<OpaqueResponseBlockingAnalyzer>(state);
}

void SanitizeBlockedResponseHeaders(network::mojom::URLResponseHead& response) {
  response.content_length = 0;
  if (response.headers)
    RemoveAllHttpResponseHeaders(response.headers);
}

}  // namespace network::orb
