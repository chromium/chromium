// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_LOG_UTIL_H_
#define NET_SPDY_SPDY_LOG_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/http/http_log_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace net {

// Given an HTTP/2 GOAWAY frame |debug_data|, returns the elided version
// according to |capture_mode|.
NET_EXPORT_PRIVATE base::Value ElideGoAwayDebugDataForNetLog(
    NetLogCaptureMode capture_mode,
    base::StringPiece debug_data);

// Given a spdy::SpdyHeaderBlock, return its base::ListValue representation.
NET_EXPORT_PRIVATE base::ListValue ElideSpdyHeaderBlockForNetLog(
    const spdy::SpdyHeaderBlock& headers,
    NetLogCaptureMode capture_mode);

// Converts a spdy::SpdyHeaderBlock into NetLog event parameters.
NET_EXPORT_PRIVATE base::Value SpdyHeaderBlockNetLogParams(
    const spdy::SpdyHeaderBlock* headers,
    NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_SPDY_SPDY_LOG_UTIL_H_
