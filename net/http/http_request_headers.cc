// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include <utility>

#include "base/logging.h"
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
    const absl::optional<base::flat_set<SourceStream::SourceType>>&
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
const char HttpRequestHeaders::kProxyAuthorization[] = "Proxy-Authorization";
const char HttpRequestHeaders::kProxyConnection[] = "Proxy-Connection";
const char HttpRequestHeaders::kRange[] = "Range";
const char HttpRequestHeaders::kReferer[] = "Referer";
const char HttpRequestHeaders::kTransferEncoding[] = "Transfer-Encoding";
const char HttpRequestHeaders::kUserAgent[] = "User-Agent";

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair() = default;

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(
    base::StringPiece key,
    base::StringPiece value)
    : HeaderKeyValuePair(key, std::string(value)) {}

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(
    base::StringPiece key,
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

bool HttpRequestHeaders::GetHeader(base::StringPiece key,
                                   std::string* out) const {
  auto it = FindHeader(key);
  if (it == headers_.end())
    return false;
  out->assign(it->value);
  return true;
}

void HttpRequestHeaders::Clear() {
  headers_.clear();
}

void HttpRequestHeaders::SetHeader(base::StringPiece key,
                                   base::StringPiece value) {
  SetHeader(key, std::string(value));
}

void HttpRequestHeaders::SetHeader(base::StringPiece key, std::string&& value) {
  // Invalid header names or values could mean clients can attach
  // browser-internal headers.
  CHECK(HttpUtil::IsValidHeaderName(key)) << key;
  CHECK(HttpUtil::IsValidHeaderValue(value)) << key << ":" << value;

  SetHeaderInternal(key, std::move(value));
}

void HttpRequestHeaders::SetHeaderWithoutCheckForTesting(
    base::StringPiece key,
    base::StringPiece value) {
  SetHeaderInternal(key, std::string(value));
}

void HttpRequestHeaders::SetHeaderIfMissing(base::StringPiece key,
                                            base::StringPiece value) {
  // Invalid header names or values could mean clients can attach
  // browser-internal headers.
  CHECK(HttpUtil::IsValidHeaderName(key));
  CHECK(HttpUtil::IsValidHeaderValue(value));
  auto it = FindHeader(key);
  if (it == headers_.end())
    headers_.push_back(HeaderKeyValuePair(key, value));
}

void HttpRequestHeaders::RemoveHeader(base::StringPiece key) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    headers_.erase(it);
}

void HttpRequestHeaders::AddHeaderFromString(base::StringPiece header_line) {
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

  const base::StringPiece header_key(header_line.data(), key_end_index);
  if (!HttpUtil::IsValidHeaderName(header_key)) {
    LOG(DFATAL) << "\"" << header_line << "\" has invalid header key.";
    return;
  }

  const std::string::size_type value_index = key_end_index + 1;

  if (value_index < header_line.size()) {
    base::StringPiece header_value(header_line.data() + value_index,
                                   header_line.size() - value_index);
    header_value = HttpUtil::TrimLWS(header_value);
    if (!HttpUtil::IsValidHeaderValue(header_value)) {
      LOG(DFATAL) << "\"" << header_line << "\" has invalid header value.";
      return;
    }
    SetHeader(header_key, header_value);
  } else if (value_index == header_line.size()) {
    SetHeader(header_key, "");
  } else {
    NOTREACHED();
  }
}

void HttpRequestHeaders::AddHeadersFromString(base::StringPiece headers) {
  for (base::StringPiece header : base::SplitStringPieceUsingSubstr(
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

base::Value HttpRequestHeaders::NetLogParams(
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
  return base::Value(std::move(dict));
}

void HttpRequestHeaders::SetAcceptEncodingIfMissing(
    const GURL& url,
    const absl::optional<base::flat_set<SourceStream::SourceType>>&
        accepted_stream_types,
    bool enable_brotli) {
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
  // Advertise "br" encoding only if transferred data is opaque to proxy.
  if (enable_brotli &&
      SupportsStreamType(accepted_stream_types,
                         SourceStream::SourceType::TYPE_BROTLI)) {
    if (url.SchemeIsCryptographic() || IsLocalhost(url)) {
      advertised_encoding_names.push_back("br");
    }
  }
  if (!advertised_encoding_names.empty()) {
    // Tell the server what compression formats are supported.
    SetHeader(
        kAcceptEncoding,
        base::JoinString(base::make_span(advertised_encoding_names), ", "));
  }
}

HttpRequestHeaders::HeaderVector::iterator HttpRequestHeaders::FindHeader(
    base::StringPiece key) {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

HttpRequestHeaders::HeaderVector::const_iterator HttpRequestHeaders::FindHeader(
    base::StringPiece key) const {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

void HttpRequestHeaders::SetHeaderInternal(base::StringPiece key,
                                           std::string&& value) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    it->value = std::move(value);
  else
    headers_.emplace_back(key, std::move(value));
}

}  // namespace net
