// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/http/http_log_util.h"
#include "net/http/http_util.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"

namespace net {

const char HttpRequestHeaders::kGetMethod[] = "GET";
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
const char HttpRequestHeaders::kSecOriginPolicy[] = "Sec-Origin-Policy";
const char HttpRequestHeaders::kTransferEncoding[] = "Transfer-Encoding";
const char HttpRequestHeaders::kUserAgent[] = "User-Agent";

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair() = default;

HttpRequestHeaders::HeaderKeyValuePair::HeaderKeyValuePair(
    const base::StringPiece& key, const base::StringPiece& value)
    : key(key.data(), key.size()), value(value.data(), value.size()) {
}


HttpRequestHeaders::Iterator::Iterator(const HttpRequestHeaders& headers)
    : started_(false),
      curr_(headers.headers_.begin()),
      end_(headers.headers_.end()) {}

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

bool HttpRequestHeaders::GetHeader(const base::StringPiece& key,
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

void HttpRequestHeaders::SetHeader(const base::StringPiece& key,
                                   const base::StringPiece& value) {
  DCHECK(HttpUtil::IsValidHeaderName(key)) << key;
  DCHECK(HttpUtil::IsValidHeaderValue(value)) << key << ":" << value;
  SetHeaderInternal(key, value);
}

void HttpRequestHeaders::SetHeaderIfMissing(const base::StringPiece& key,
                                            const base::StringPiece& value) {
  DCHECK(HttpUtil::IsValidHeaderName(key));
  DCHECK(HttpUtil::IsValidHeaderValue(value));
  auto it = FindHeader(key);
  if (it == headers_.end())
    headers_.push_back(HeaderKeyValuePair(key, value));
}

void HttpRequestHeaders::RemoveHeader(const base::StringPiece& key) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    headers_.erase(it);
}

void HttpRequestHeaders::AddHeaderFromString(
    const base::StringPiece& header_line) {
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

void HttpRequestHeaders::AddHeadersFromString(
    const base::StringPiece& headers) {
  for (const base::StringPiece& header : base::SplitStringPieceUsingSubstr(
           headers, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    AddHeaderFromString(header);
  }
}

void HttpRequestHeaders::MergeFrom(const HttpRequestHeaders& other) {
  for (auto it = other.headers_.begin(); it != other.headers_.end(); ++it) {
    SetHeader(it->key, it->value);
  }
}

std::string HttpRequestHeaders::ToString() const {
  std::string output;
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    base::StringAppendF(&output, "%s: %s\r\n", it->key.c_str(),
                        it->value.c_str());
  }
  output.append("\r\n");
  return output;
}

base::Value HttpRequestHeaders::NetLogParams(
    const std::string& request_line,
    NetLogCaptureMode capture_mode) const {
  base::DictionaryValue dict;
  dict.SetKey("line", NetLogStringValue(request_line));
  auto headers = std::make_unique<base::ListValue>();
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    std::string log_value =
        ElideHeaderValueForNetLog(capture_mode, it->key, it->value);
    headers->Append(
        NetLogStringValue(base::StrCat({it->key, ": ", log_value})));
  }
  dict.Set("headers", std::move(headers));
  return std::move(dict);
}

HttpRequestHeaders::HeaderVector::iterator
HttpRequestHeaders::FindHeader(const base::StringPiece& key) {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

HttpRequestHeaders::HeaderVector::const_iterator
HttpRequestHeaders::FindHeader(const base::StringPiece& key) const {
  for (auto it = headers_.begin(); it != headers_.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(key, it->key))
      return it;
  }

  return headers_.end();
}

void HttpRequestHeaders::SetHeaderInternal(const base::StringPiece& key,
                                           const base::StringPiece& value) {
  auto it = FindHeader(key);
  if (it != headers_.end())
    it->value.assign(value.data(), value.size());
  else
    headers_.push_back(HeaderKeyValuePair(key, value));
}

}  // namespace net
