// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "net/http/http_log_util.h"
#include "net/http/http_util.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {

bool SupportsStreamType(
    const std::optional<base::flat_set<SourceStream::SourceType>>&
        accepted_stream_types,
    SourceStream::SourceType type) {
  if (!accepted_stream_types)
    return true;
  return accepted_stream_types->contains(type);
}

}  // namespace

const char HttpRequestHeaders::kConnectMethod[] = "CONNECT";
const char HttpRequestHeaders::kDeleteMethod[] = "DELETE";
const char HttpRequestHeaders::kGetMethod[] = "GET";
const char HttpRequestHeaders::kHeadMethod[] = "HEAD";
const char HttpRequestHeaders::kOptionsMethod[] = "OPTIONS";
const char HttpRequestHeaders::kPatchMethod[] = "PATCH";
const char HttpRequestHeaders::kPostMethod[] = "POST";
const char HttpRequestHeaders::kPutMethod[] = "PUT";
const char HttpRequestHeaders::kTraceMethod[] = "TRACE";
const char HttpRequestHeaders::kTrackMethod[] = "TRACK";
const char HttpRequestHeaders::kAccept[] = "Accept";
const char HttpRequestHeaders::kAcceptCharset[] = "Accept-Charset";
const char HttpRequestHeaders::kAcceptEncoding[] = "Accept-Encoding";
const char HttpRequestHeaders::kAcceptLanguage[] = "Accept-Language";
const char HttpRequestHeaders::kAuthorization[] = "Authorization";
const char HttpRequestHeaders::kCacheControl[] = "Cache-Control";
const char HttpRequestHeaders::kConnection[] = "Connection";
const char HttpRequestHeaders::kContentLength[] = "Content-Length";
const char HttpRequestHeaders::kContentType[] = "Content-Type";
const char HttpRequestHeaders::kCookie[] = "Cookie";
const char HttpRequestHeaders::kHost[] = "Host";
const char HttpRequestHeaders::kIfMatch[] = "If-Match";
const char HttpRequestHeaders::kIfModifiedSince[] = "If-Modified-Since";
const char HttpRequestHeaders::kIfNoneMatch[] = "If-None-Match";
const char HttpRequestHeaders::kIfRange[] = "If-Range";
const char HttpRequestHeaders::kIfUnmodifiedSince[] = "If-Unmodified-Since";
const char HttpRequestHeaders::kOrigin[] = "Origin";
const char HttpRequestHeaders::kPragma[] = "Pragma";
const char HttpRequestHeaders::kPriority[] = "Priority";
const char HttpRequestHeaders::kProxyAuthorization[] = "Proxy-Authorization";
const char HttpRequestHeaders::kProxyConnection[] = "Proxy-Connection";
const char HttpRequestHeaders::kRange[] = "Range";
const char HttpRequestHeaders::kReferer[] = "Referer";
const char HttpRequestHeaders::kSecFetchStorageAccess[] =
    "Sec-Fetch-Storage-Access";
const char HttpRequestHeaders::kTransferEncoding[] = "Transfer-Encoding";
const char HttpRequestHeaders::kUserAgent[] = "User-Agent";

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair() = default;

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(
    std::string_view key,
    std::string_view value)
    : HeaderKeyValuePair(key, std::string(value)) {}

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(std::string_view key,
                                                           std::string&& value)
    : key(key), value(std::move(value)) {}

HttpRequestHeaders::Iterator::Iterator(const HttpRequestHeaders& headers)
    : curr_(headers.headers_.begin()), end_(headers.headers_.end()) {}

HttpRequestHeaders::Iterator::~Iterator() = default;

bool HttpRequestHeaders::Iterator::GetNext() {
  if (!started_) {
    started_ = true;
    return curr_ != end_;
  }

  if (curr_ == end_)
    return false;

  ++curr_;
  return curr_ != end_;
}

HttpRequestHeaders::HttpRequestHeaders() = default;
HttpRequestHeaders::HttpRequestHeaders(const HttpRequestHeaders& other) =
    default;
HttpRequestHeaders::HttpRequestHeaders(HttpRequestHeaders&& other) = default;
HttpRequestHeaders::~HttpRequestHeaders() = default;

HttpRequestHeaders& HttpRequestHeaders::operator=(
    const HttpRequestHeaders& other) = default;
HttpRequestHeaders& HttpRequestHeaders::operator=(HttpRequestHeaders&& other) =
    default;

std::optional<std::string> HttpRequestHeaders::GetHeader(
    std::string_view key) const {
  auto it = FindHeader(key);
  if (it == headers_.end())
    return std::nullopt;
  return it->value;
}

void HttpRequestHeaders::Clear() {
  headers_.clear();
}

void HttpRequestHeaders::SetHeader(std::string_view key,
                                   std::string_view value) {
  SetHeader(key, std::string(value));
}

void HttpRequestHeaders::SetHeader(std::string_view key, std::string&& value) {
  // Invalid header names or values could mean clients can attach
  // browser-internal headers.
  CHECK(HttpUtil::IsValidHeaderName(key)) << key;
  CHECK(HttpUtil::IsValidHeaderValue(value)) << key << " has invalid value.";

  SetHeaderInternal(key, std::move(value));
}

void HttpRequestHeaders::SetHeaderWithoutCheckForTesting(
    std::string_view key,
    std::string_view value) {
  SetHeaderInternal(key, std::string(value));
}

void HttpRequestHeaders::SetHeaderIfMissing(std::string_view key,
                                            std::string_view value) {
  // Invalid header names or values could mean clients can attach
  // browser-internal headers.
  CHECK(HttpUtil::IsValidHeaderName(key));
  CHECK(HttpUtil::IsValidHeaderValue(value));
  auto it = FindHeader(key);
  if (it == headers_.end())
    headers_.push_back(HeaderKeyValuePair(key, value));
}

void HttpRequestHeaders::RemoveHeader(std::string_view key) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    headers_.erase(it);
}

void HttpRequestHeaders::AddHeaderFromString(std::string_view header_line) {
  DCHECK_EQ(std::string::npos, header_line.find("\r\n"))
      << "\"" << header_line << "\" contains CRLF.";

  const std::string::size_type key_end_index = header_line.find(":");
  if (key_end_index == std::string::npos) {
    LOG(DFATAL) << "\"" << header_line << "\" is missing colon delimiter.";
    return;
  }

  if (key_end_index == 0) {
    LOG(DFATAL) << "\"" << header_line << "\" is missing header key.";
    return;
  }

  const std::string_view header_key = header_line.substr(0, key_end_index);
  if (!HttpUtil::IsValidHeaderName(header_key)) {
    LOG(DFATAL) << "\"" << header_line << "\" has invalid header key.";
    return;
  }

  const std::string::size_type value_index = key_end_index + 1;

  if (value_index < header_line.size()) {
    std::string_view header_value = header_line.substr(value_index);
    header_value = HttpUtil::TrimLWS(header_value);
    if (!HttpUtil::IsValidHeaderValue(header_value)) {
      LOG(DFATAL) << "\"" << header_line << "\" has invalid header value.";
      return;
    }
    SetHeader(header_key, header_value);
  } else if (value_index == header_line.size()) {
    SetHeader(header_key, "");
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void HttpRequestHeaders::AddHeadersFromString(std::string_view headers) {
  for (std::string_view header : base::SplitStringPieceUsingSubstr(
           headers, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    AddHeaderFromString(header);
  }
}

void HttpRequestHeaders::MergeFrom(const HttpRequestHeaders& other) {
  for (const auto& header : other.headers_) {
    SetHeader(header.key, header.value);
  }
}

std::string HttpRequestHeaders::ToString() const {
  std::string output;
  for (const auto& header : headers_) {
    base::StringAppendF(&output, "%s: %s\r\n", header.key.c_str(),
                        header.value.c_str());
  }
  output.append("\r\n");
  return output;
}

base::Value::Dict HttpRequestHeaders::NetLogParams(
    const std::string& request_line,
    NetLogCaptureMode capture_mode) const {
  base::Value::Dict dict;
  dict.Set("line", NetLogStringValue(request_line));
  base::Value::List headers;
  for (const auto& header : headers_) {
    std::string log_value =
        ElideHeaderValueForNetLog(capture_mode, header.key, header.value);
    headers.Append(
        NetLogStringValue(base::StrCat({header.key, ": ", log_value})));
  }
  dict.Set("headers", std::move(headers));
  return dict;
}

void HttpRequestHeaders::SetAcceptEncodingIfMissing(
    const GURL& url,
    const std::optional<base::flat_set<SourceStream::SourceType>>&
        accepted_stream_types,
    bool enable_brotli,
    bool enable_zstd) {
  if (HasHeader(kAcceptEncoding))
    return;

  // If a range is specifically requested, set the "Accepted Encoding" header to
  // "identity".
  if (HasHeader(kRange)) {
    SetHeader(kAcceptEncoding, "identity");
    return;
  }

  // Supply Accept-Encoding headers first so that it is more likely that they
  // will be in the first transmitted packet. This can sometimes make it easier
  // to filter and analyze the streams to assure that a proxy has not damaged
  // these headers. Some proxies deliberately corrupt Accept-Encoding headers.
  std::vector<std::string> advertised_encoding_names;
  if (SupportsStreamType(accepted_stream_types,
                         SourceStream::SourceType::TYPE_GZIP)) {
    advertised_encoding_names.push_back("gzip");
  }
  if (SupportsStreamType(accepted_stream_types,
                         SourceStream::SourceType::TYPE_DEFLATE)) {
    advertised_encoding_names.push_back("deflate");
  }

  const bool can_use_advanced_encodings =
      (url.SchemeIsCryptographic() || IsLocalhost(url));

  // Advertise "br" encoding only if transferred data is opaque to proxy.
  if (enable_brotli &&
      SupportsStreamType(accepted_stream_types,
                         SourceStream::SourceType::TYPE_BROTLI) &&
      can_use_advanced_encodings) {
    advertised_encoding_names.push_back("br");
  }
  // Advertise "zstd" encoding only if transferred data is opaque to proxy.
  if (enable_zstd &&
      SupportsStreamType(accepted_stream_types,
                         SourceStream::SourceType::TYPE_ZSTD) &&
      can_use_advanced_encodings) {
    advertised_encoding_names.push_back("zstd");
  }
  if (!advertised_encoding_names.empty()) {
    // Tell the server what compression formats are supported.
    SetHeader(
        kAcceptEncoding,
        base::JoinString(base::make_span(advertised_encoding_names), ", "));
  }
}

HttpRequestHeaders::HeaderVector::iterator HttpRequestHeaders::FindHeader(
    std::string_view key) {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

HttpRequestHeaders::HeaderVector::const_iterator HttpRequestHeaders::FindHeader(
    std::string_view key) const {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

void HttpRequestHeaders::SetHeaderInternal(std::string_view key,
                                           std::string&& value) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    it->value = std::move(value);
  else
    headers_.emplace_back(key, std::move(value));
}

}  // namespace net
