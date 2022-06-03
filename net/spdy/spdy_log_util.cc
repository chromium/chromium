// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_log_util.h"

#include <utility>

#include "base/strings/abseil_string_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/http/http_log_util.h"
#include "net/log/net_log_values.h"

namespace net {

base::Value ElideGoAwayDebugDataForNetLog(NetLogCaptureMode capture_mode,
                                          base::StringPiece debug_data) {
  if (NetLogCaptureIncludesSensitive(capture_mode))
    return NetLogStringValue(debug_data);

  return NetLogStringValue(base::StrCat(
      {"[", base::NumberToString(debug_data.size()), " bytes were stripped]"}));
}

base::ListValue ElideHttp2HeaderBlockForNetLog(
    const spdy::Http2HeaderBlock& headers,
    NetLogCaptureMode capture_mode) {
  base::ListValue headers_list;
  for (const auto& header : headers) {
    base::StringPiece key = base::StringViewToStringPiece(header.first);
    base::StringPiece value = base::StringViewToStringPiece(header.second);
    headers_list.Append(NetLogStringValue(
        base::StrCat({key, ": ",
                      ElideHeaderValueForNetLog(capture_mode, std::string(key),
                                                std::string(value))})));
  }
  return headers_list;
}

base::Value Http2HeaderBlockNetLogParams(const spdy::Http2HeaderBlock* headers,
                                         NetLogCaptureMode capture_mode) {
  base::DictionaryValue dict;
  dict.SetKey("headers",
              ElideHttp2HeaderBlockForNetLog(*headers, capture_mode));
  return std::move(dict);
}

}  // namespace net
