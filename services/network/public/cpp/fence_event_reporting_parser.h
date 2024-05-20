// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FENCE_EVENT_REPORTING_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FENCE_EVENT_REPORTING_PARSER_H_

#include "base/component_export.h"
#include "net/http/http_response_headers.h"

namespace network {

// Parses the `Allow-Cross-Origin-Event-Reporting` response header. Returns true
// if the parsing succeeds and the parsed value is Boolean true; returns false
// otherwise. See:
// https://wicg.github.io/fenced-frame/#fenced-frame-config-cross-origin-reporting-allowed
COMPONENT_EXPORT(NETWORK_CPP)
bool ParseAllowCrossOriginEventReportingFromHeader(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FENCE_EVENT_REPORTING_PARSER_H_
