// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/header_coalescer.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/http/http_log_util.h"
#include "net/http/http_util.h"
#include "net/log/net_log_values.h"

namespace net {
namespace {

void NetLogInvalidHeader(const NetLogWithSource& net_log,
                         std::string_view header_name,
                         std::string_view header_value,
                         const char* error_message) {
  net_log.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_INVALID_HEADER,
                   [&](NetLogCaptureMode capture_mode) {
                     return base::Value::Dict()
                         .Set("header_name", NetLogStringValue(header_name))
                         .Set("header_value",
                              NetLogStringValue(ElideHeaderValueForNetLog(
                                  capture_mode, std::string(header_name),
                                  std::string(header_value))))
                         .Set("error", error_message);
                   });
}

bool ContainsUppercaseAscii(std::string_view str) {
  return base::ranges::any_of(str, base::IsAsciiUpper<char>);
}

}  // namespace

HeaderCoalescer::HeaderCoalescer(uint32_t max_header_list_size,
                                 const NetLogWithSource& net_log)
    : max_header_list_size_(max_header_list_size), net_log_(net_log) {}

void HeaderCoalescer::OnHeader(std::string_view key, std::string_view value) {
  if (error_seen_)
    return;
  if (!AddHeader(key, value)) {
    error_seen_ = true;
  }
}

quiche::HttpHeaderBlock HeaderCoalescer::release_headers() {
  DCHECK(headers_valid_);
  headers_valid_ = false;
  return std::move(headers_);
}

bool HeaderCoalescer::AddHeader(std::string_view key, std::string_view value) {
  if (key.empty()) {
    NetLogInvalidHeader(net_log_, key, value, "Header name must not be empty.");
    return false;
  }

  std::string_view key_name = key;
  if (key[0] == ':') {
    if (regular_header_seen_) {
      NetLogInvalidHeader(net_log_, key, value,
                          "Pseudo header must not follow regular headers.");
      return false;
    }
    key_name.remove_prefix(1);
  } else if (!regular_header_seen_) {
    regular_header_seen_ = true;
  }

  if (!HttpUtil::IsValidHeaderName(key_name)) {
    NetLogInvalidHeader(net_log_, key, value,
                        "Invalid character in header name.");
    return false;
  }

  if (ContainsUppercaseAscii(key_name)) {
    NetLogInvalidHeader(net_log_, key, value,
                        "Upper case characters in header name.");
    return false;
  }

  // 32 byte overhead according to RFC 7540 Section 6.5.2.
  header_list_size_ += key.size() + value.size() + 32;
  if (header_list_size_ > max_header_list_size_) {
    NetLogInvalidHeader(net_log_, key, value, "Header list too large.");
    return false;
  }

  // RFC 7540 Section 10.3: "Any request or response that contains a character
  // not permitted in a header field value MUST be treated as malformed (Section
  // 8.1.2.6). Valid characters are defined by the field-content ABNF rule in
  // Section 3.2 of [RFC7230]." RFC 7230 Section 3.2 says:
  // field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
  // field-vchar    = VCHAR / obs-text
  // RFC 5234 Appendix B.1 defines |VCHAR|:
  // VCHAR          =  %x21-7E
  // RFC 7230 Section 3.2.6 defines |obs-text|:
  // obs-text       = %x80-FF
  // Therefore allowed characters are '\t' (HTAB), x20 (SP), x21-7E, and x80-FF.
  for (const unsigned char c : value) {
    if (c < '\t' || ('\t' < c && c < 0x20) || c == 0x7f) {
      std::string error_line;
      base::StringAppendF(&error_line,
                          "Invalid character 0x%02X in header value.", c);
      NetLogInvalidHeader(net_log_, key, value, error_line.c_str());
      return false;
    }
  }

  headers_.AppendValueOrAddHeader(key, value);
  return true;
}

}  // namespace net
