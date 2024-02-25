// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/browsing_topics_parser.h"

#include "net/http/structured_headers.h"

namespace network {

bool ParseObserveBrowsingTopicsFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::string header_value;
  headers.GetNormalizedHeader("Observe-Browsing-Topics", &header_value);
  std::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(header_value);
  return item && item->is_boolean() && item->GetBoolean();
}

}  // namespace network
