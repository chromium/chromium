// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_http_utils.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream_priority.h"

namespace net {

const char* const kHttp2PriorityHeader = "priority";

namespace {

// The number of bytes to reserve for the raw headers string to avoid having to
// do reallocations most of the time. Equal to the 99th percentile of header
// sizes in ricea@'s cache on 3 Aug 2023.
constexpr size_t kExpectedRawHeaderSize = 4035;

// Add header `name` with `value` to `headers`. `name` must not already exist in
// `headers`.
void AddUniqueSpdyHeader(std::string_view name,
                         std::string_view value,
                         quiche::HttpHeaderBlock* headers) {
  auto insert_result = headers->insert({name, value});
  CHECK_EQ(insert_result, quiche::HttpHeaderBlock::InsertResult::kInserted);
}

// Convert `headers` to an HttpResponseHeaders object based on the features
// enabled at runtime.
base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingFeatures(
    const quiche::HttpHeaderBlock& headers) {
  if (base::FeatureList::IsEnabled(
          features::kSpdyHeadersToHttpResponseUseBuilder)) {
    return SpdyHeadersToHttpResponseHeadersUsingBuilder(headers);
  } else {
    return SpdyHeadersToHttpResponseHeadersUsingRawString(headers);
  }
}

}  // namespace

int SpdyHeadersToHttpResponse(const quiche::HttpHeaderBlock& headers,
                              HttpResponseInfo* response) {
  ASSIGN_OR_RETURN(response->headers,
                   SpdyHeadersToHttpResponseHeadersUsingFeatures(headers));
  response->was_fetched_via_spdy = true;
  return OK;
}

NET_EXPORT_PRIVATE base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingRawString(
    const quiche::HttpHeaderBlock& headers) {
  // The ":status" header is required.
  quiche::HttpHeaderBlock::const_iterator it =
      headers.find(spdy::kHttp2StatusHeader);
  if (it == headers.end()) {
    return base::unexpected(ERR_INCOMPLETE_HTTP2_HEADERS);
  }

  const auto status = it->second;

  std::string raw_headers =
      base::StrCat({"HTTP/1.1 ", status, std::string_view("\0", 1)});
  raw_headers.reserve(kExpectedRawHeaderSize);
  for (const auto& [name, value] : headers) {
    DCHECK_GT(name.size(), 0u);
    if (name[0] == ':') {
      // https://tools.ietf.org/html/rfc7540#section-8.1.2.4
      // Skip pseudo headers.
      continue;
    }
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string_view tval;
      if (end != value.npos) {
        tval = value.substr(start, (end - start));
      } else {
        tval = value.substr(start);
      }
      base::StrAppend(&raw_headers,
                      {name, ":", tval, std::string_view("\0", 1)});
      start = end + 1;
    } while (end != value.npos);
  }

  auto response_headers =
      base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  // When there are multiple location headers the response is a potential
  // response smuggling attack.
  if (HttpUtil::HeadersContainMultipleCopiesOfField(*response_headers,
                                                    "location")) {
    return base::unexpected(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION);
  }

  return response_headers;
}

NET_EXPORT_PRIVATE base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingBuilder(
    const quiche::HttpHeaderBlock& headers) {
  // The ":status" header is required.
  // TODO(ricea): The ":status" header should always come first. Skip this hash
  // lookup after we no longer need to be compatible with the old
  // implementation.
  quiche::HttpHeaderBlock::const_iterator it =
      headers.find(spdy::kHttp2StatusHeader);
  if (it == headers.end()) {
    return base::unexpected(ERR_INCOMPLETE_HTTP2_HEADERS);
  }

  const auto status = it->second;

  HttpResponseHeaders::Builder builder({1, 1}, status);

  for (const auto& [name, value] : headers) {
    DCHECK_GT(name.size(), 0u);
    if (name[0] == ':') {
      // https://tools.ietf.org/html/rfc7540#section-8.1.2.4
      // Skip pseudo headers.
      continue;
    }
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    size_t start = 0;
    size_t end = 0;
    std::optional<std::string_view> location_value;
    do {
      end = value.find('\0', start);
      std::string_view tval;
      if (end != value.npos) {
        tval = value.substr(start, (end - start));

        // TODO(ricea): Make this comparison case-sensitive when we are no
        // longer maintaining compatibility with the old version of the
        // function.
        if (base::EqualsCaseInsensitiveASCII(name, "location") &&
            !location_value.has_value()) {
          location_value = HttpUtil::TrimLWS(tval);
        }
      } else {
        tval = value.substr(start);
      }
      if (location_value.has_value() && start > 0) {
        DCHECK(base::EqualsCaseInsensitiveASCII(name, "location"));
        std::string_view trimmed_value = HttpUtil::TrimLWS(tval);
        if (trimmed_value != location_value.value()) {
          return base::unexpected(ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION);
        }
      }
      builder.AddHeader(name, tval);
      start = end + 1;
    } while (end != value.npos);
  }

  return builder.Build();
}

void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      std::optional<RequestPriority> priority,
                                      const HttpRequestHeaders& request_headers,
                                      quiche::HttpHeaderBlock* headers) {
  headers->insert({spdy::kHttp2MethodHeader, info.method});
  if (info.method == "CONNECT") {
    headers->insert({spdy::kHttp2AuthorityHeader, GetHostAndPort(info.url)});
  } else {
    headers->insert(
        {spdy::kHttp2AuthorityHeader, GetHostAndOptionalPort(info.url)});
    headers->insert({spdy::kHttp2SchemeHeader, info.url.scheme()});
    headers->insert({spdy::kHttp2PathHeader, info.url.PathForRequest()});
  }

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "connection" ||
        name == "proxy-connection" || name == "transfer-encoding" ||
        name == "host") {
      continue;
    }
    AddUniqueSpdyHeader(name, it.value(), headers);
  }

  // Add the priority header if there is not already one set. This uses the
  // quic helpers but the header values for HTTP extensible priorities are
  // independent of quic.
  if (priority &&
      base::FeatureList::IsEnabled(net::features::kPriorityHeader) &&
      headers->find(kHttp2PriorityHeader) == headers->end()) {
    uint8_t urgency = ConvertRequestPriorityToQuicPriority(priority.value());
    bool incremental = info.priority_incremental;
    quic::HttpStreamPriority quic_priority{urgency, incremental};
    std::string serialized_priority =
        quic::SerializePriorityFieldValue(quic_priority);
    if (!serialized_priority.empty()) {
      AddUniqueSpdyHeader(kHttp2PriorityHeader, serialized_priority, headers);
    }
  }
}

void CreateSpdyHeadersFromHttpRequestForExtendedConnect(
    const HttpRequestInfo& info,
    std::optional<RequestPriority> priority,
    const std::string& ext_connect_protocol,
    const HttpRequestHeaders& request_headers,
    quiche::HttpHeaderBlock* headers) {
  CHECK_EQ(info.method, "CONNECT");

  // Extended CONNECT, unlike CONNECT, requires scheme and path, and uses the
  // default port in the authority header.
  headers->insert({spdy::kHttp2SchemeHeader, info.url.scheme()});
  headers->insert({spdy::kHttp2PathHeader, info.url.PathForRequest()});
  headers->insert({spdy::kHttp2ProtocolHeader, ext_connect_protocol});

  CreateSpdyHeadersFromHttpRequest(info, priority, request_headers, headers);

  // Replace the existing `:authority` header. This will still be ordered
  // correctly, since the header was first added before any regular headers.
  headers->insert(
      {spdy::kHttp2AuthorityHeader, GetHostAndOptionalPort(info.url)});
}

void CreateSpdyHeadersFromHttpRequestForWebSocket(
    const GURL& url,
    const HttpRequestHeaders& request_headers,
    quiche::HttpHeaderBlock* headers) {
  headers->insert({spdy::kHttp2MethodHeader, "CONNECT"});
  headers->insert({spdy::kHttp2AuthorityHeader, GetHostAndOptionalPort(url)});
  headers->insert({spdy::kHttp2SchemeHeader, "https"});
  headers->insert({spdy::kHttp2PathHeader, url.PathForRequest()});
  headers->insert({spdy::kHttp2ProtocolHeader, "websocket"});

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "upgrade" ||
        name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding" || name == "host") {
      continue;
    }
    AddUniqueSpdyHeader(name, it.value(), headers);
  }
}

static_assert(HIGHEST - LOWEST < 4 && HIGHEST - MINIMUM_PRIORITY < 6,
              "request priority incompatible with spdy");

spdy::SpdyPriority ConvertRequestPriorityToSpdyPriority(
    const RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  return static_cast<spdy::SpdyPriority>(MAXIMUM_PRIORITY - priority +
                                         spdy::kV3HighestPriority);
}

NET_EXPORT_PRIVATE RequestPriority
ConvertSpdyPriorityToRequestPriority(spdy::SpdyPriority priority) {
  // Handle invalid values gracefully.
  return ((priority - spdy::kV3HighestPriority) >
          (MAXIMUM_PRIORITY - MINIMUM_PRIORITY))
             ? IDLE
             : static_cast<RequestPriority>(
                   MAXIMUM_PRIORITY - (priority - spdy::kV3HighestPriority));
}

NET_EXPORT_PRIVATE void ConvertHeaderBlockToHttpRequestHeaders(
    const quiche::HttpHeaderBlock& spdy_headers,
    HttpRequestHeaders* http_headers) {
  for (const auto& it : spdy_headers) {
    std::string_view key = it.first;
    if (key[0] == ':') {
      key.remove_prefix(1);
    }
    std::vector<std::string_view> values = base::SplitStringPiece(
        it.second, "\0", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const auto& value : values) {
      http_headers->SetHeader(key, value);
    }
  }
}

}  // namespace net
