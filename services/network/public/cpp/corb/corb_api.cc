// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/corb/corb_api.h"

#include <string>
#include <unordered_set>

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/corb/corb_impl.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {
namespace corb {

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
std::unique_ptr<ResponseAnalyzer> ResponseAnalyzer::Create() {
  // TODO(https://crbug.com/1178928): Instead of always returning a CORB-based
  // implementation, consult base::FeatureList and return an ORB-based
  // implementation if needed.
  return std::make_unique<CrossOriginReadBlocking::CorbResponseAnalyzer>();
}

void SanitizeBlockedResponseHeaders(network::mojom::URLResponseHead& response) {
  response.content_length = 0;
  if (response.headers)
    RemoveAllHttpResponseHeaders(response.headers);
}

}  // namespace corb
}  // namespace network
