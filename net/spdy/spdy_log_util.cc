// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_log_util.h"

#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/http/http_log_util.h"
#include "net/log/net_log_values.h"

namespace net {

base::Value ElideGoAwayDebugDataForNetLog(NetLogCaptureMode capture_mode,
                                          std::string_view debug_data) {
  if (NetLogCaptureIncludesSensitive(capture_mode))
    return NetLogStringValue(debug_data);

  return NetLogStringValue(base::StrCat(
      {"[", base::NumberToString(debug_data.size()), " bytes were stripped]"}));
}

base::Value::List ElideHttpHeaderBlockForNetLog(
    const quiche::HttpHeaderBlock& headers,
    NetLogCaptureMode capture_mode) {
  base::Value::List headers_list;
  for (const auto& [key, value] : headers) {
    headers_list.Append(NetLogStringValue(
        base::StrCat({key, ": ",
                      ElideHeaderValueForNetLog(capture_mode, std::string(key),
                                                std::string(value))})));
  }
  return headers_list;
}

base::Value::Dict HttpHeaderBlockNetLogParams(
    const quiche::HttpHeaderBlock* headers,
    NetLogCaptureMode capture_mode) {
  return base::Value::Dict().Set(
      "headers", ElideHttpHeaderBlockForNetLog(*headers, capture_mode));
}

}  // namespace net
