// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/browsing_topics_parser.h"

#include "net/http/structured_headers.h"

namespace network {

bool ParseObserveBrowsingTopicsFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::string header_value =
      headers.GetNormalizedHeader("Observe-Browsing-Topics")
          .value_or(std::string());
  std::optional<net::structured_headers::ParameterizedItem> item =
      net::structured_headers::ParseItem(header_value);
  return item && item->item.is_boolean() && item->item.GetBoolean();
}

}  // namespace network
