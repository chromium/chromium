// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/fence_event_reporting_parser.h"

#include "net/http/structured_headers.h"

namespace network {

bool ParseAllowCrossOriginEventReportingFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::string header_value;
  headers.GetNormalizedHeader("Allow-Cross-Origin-Event-Reporting",
                              &header_value);
  std::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(header_value);
  return item && item->is_boolean() && item->GetBoolean();
}

}  // namespace network
