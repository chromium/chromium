// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_LOG_UTIL_H_
#define NET_HTTP_HTTP_LOG_UTIL_H_

#include <string>

#include "net/base/net_export.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"

namespace net {

class NetLogWithSource;
class HttpResponseHeaders;
class HttpRequestHeaders;

// Given an HTTP header |header| with value |value|, returns the elided version
// of the header value at |log_level|.
NET_EXPORT_PRIVATE std::string ElideHeaderValueForNetLog(
    NetLogCaptureMode capture_mode,
    const std::string& header,
    const std::string& value);

NET_EXPORT void NetLogResponseHeaders(const NetLogWithSource& net_log,
                                      NetLogEventType type,
                                      const HttpResponseHeaders* headers);

NET_EXPORT void NetLogRequestHeaders(const NetLogWithSource& net_log,
                                     NetLogEventType type,
                                     const std::string& request_line,
                                     const HttpRequestHeaders* headers);

}  // namespace net

#endif  // NET_HTTP_HTTP_LOG_UTIL_H_
