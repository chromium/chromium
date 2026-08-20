// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/fence_event_reporting_parser.h"

#include "net/http/structured_headers.h"

namespace network {

bool ParseAllowCrossOriginEventReportingFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::optional<std::string> header_value =
      headers.GetNormalizedHeader("Allow-Cross-Origin-Event-Reporting");
  if (!header_value.has_value()) {
    return false;
  }

  std::optional<net::structured_headers::ParameterizedItem> item =
      net::structured_headers::ParseItem(*header_value);
  if (!item) {
    return false;
  }
  const bool* value = item->item.GetIfBoolean();
  return value && *value;
}

}  // namespace network
