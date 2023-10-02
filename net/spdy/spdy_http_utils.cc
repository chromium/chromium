// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <string>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/quic/quic_http_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream_priority.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"

namespace net {

const char* const kHttp2PriorityHeader = "priority";

namespace {

// The number of bytes to reserve for the raw headers string to avoid having to
// do reallocations most of the time. Equal to the 99th percentile of header
// sizes in ricea@'s cache on 3 Aug 2023.
constexpr size_t kExpectedRawHeaderSize = 4035;

void AddSpdyHeader(const std::string& name,
                   const std::string& value,
                   spdy::Http2HeaderBlock* headers) {
  if (headers->find(name) == headers->end()) {
    (*headers)[name] = value;
  } else {
    (*headers)[name] = base::StrCat(
        {(*headers)[name].as_string(), base::StringPiece("\0", 1), value});
  }
}

// Tries both the old and new implementations of
// SpdyHeadersToHttpResponseHeaders() and creates a crash report if they do not
// match. Always returns the results of the old implementation, so behavior is
// unchanged (except for performance).
base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersVerifyingCorrectness(
    const spdy::Http2HeaderBlock& headers) {
  auto using_builder = SpdyHeadersToHttpResponseHeadersUsingBuilder(headers);
  auto using_raw_string =
      SpdyHeadersToHttpResponseHeadersUsingRawString(headers);
  // If the code is working correctly, it shouldn't be possible to hit any of
  // the DumpWithoutCrashing() conditions in this function, and so they will not
  // have code coverage.
  if (using_builder.has_value() != using_raw_string.has_value()) {
    SCOPED_CRASH_KEY_BOOL("spdy", "builder_has", using_builder.has_value());
    SCOPED_CRASH_KEY_BOOL("spdy", "raw_has", using_raw_string.has_value());
    base::debug::DumpWithoutCrashing();
  } else if (!using_builder.has_value()) {
    if (using_builder.error() != using_raw_string.error()) {
      SCOPED_CRASH_KEY_NUMBER("spdy", "builder_err", using_builder.error());
      SCOPED_CRASH_KEY_NUMBER("spdy", "raw_err", using_raw_string.error());
      base::debug::DumpWithoutCrashing();
    }
  } else {
    if (!using_builder.value()->StrictlyEquals(*using_raw_string.value())) {
      // We will have to add some diagnostics here if this actually triggers in
      // practice. The privacy issues are complex so don't do anything yet.
      base::debug::DumpWithoutCrashing();
    }
  }
  return using_raw_string;
}

// Convert `headers` to an HttpResponseHeaders object based on the features
// enabled at runtime.
base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingFeatures(
    const spdy::Http2HeaderBlock& headers) {
  if (base::FeatureList::IsEnabled(
          features::kSpdyHeadersToHttpResponseUseBuilder)) {
    return SpdyHeadersToHttpResponseHeadersUsingBuilder(headers);
  } else if (base::FeatureList::IsEnabled(
                 features::kSpdyHeadersToHttpResponseVerifyCorrectness)) {
    return SpdyHeadersToHttpResponseHeadersVerifyingCorrectness(headers);
  } else {
    return SpdyHeadersToHttpResponseHeadersUsingRawString(headers);
  }
}

}  // namespace

int SpdyHeadersToHttpResponse(const spdy::Http2HeaderBlock& headers,
                              HttpResponseInfo* response) {
  ASSIGN_OR_RETURN(response->headers,
                   SpdyHeadersToHttpResponseHeadersUsingFeatures(headers));
  response->was_fetched_via_spdy = true;
  return OK;
}

NET_EXPORT_PRIVATE base::expected<scoped_refptr<HttpResponseHeaders>, int>
SpdyHeadersToHttpResponseHeadersUsingRawString(
    const spdy::Http2HeaderBlock& headers) {
  // The ":status" header is required.
  spdy::Http2HeaderBlock::const_iterator it =
      headers.find(spdy::kHttp2StatusHeader);
  if (it == headers.end())
    return base::unexpected(ERR_INCOMPLETE_HTTP2_HEADERS);

  const auto status = it->second;

  std::string raw_headers =
      base::StrCat({"HTTP/1.1 ", status, base::StringPiece("\0", 1)});
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
      base::StringPiece tval;
      if (end != value.npos) {
        tval = value.substr(start, (end - start));
      } else {
        tval = value.substr(start);
      }
      base::StrAppend(&raw_headers,
                      {name, ":", tval, base::StringPiece("\0", 1)});
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
    const spdy::Http2HeaderBlock& headers) {
  // The ":status" header is required.
  // TODO(ricea): The ":status" header should always come first. Skip this hash
  // lookup after we no longer need to be compatible with the old
  // implementation.
  spdy::Http2HeaderBlock::const_iterator it =
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
    absl::optional<base::StringPiece> location_value;
    do {
      end = value.find('\0', start);
      base::StringPiece tval;
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
        base::StringPiece trimmed_value = HttpUtil::TrimLWS(tval);
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
                                      absl::optional<RequestPriority> priority,
                                      const HttpRequestHeaders& request_headers,
                                      spdy::Http2HeaderBlock* headers) {
  (*headers)[spdy::kHttp2MethodHeader] = info.method;
  if (info.method == "CONNECT") {
    (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndPort(info.url);
  } else {
    (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndOptionalPort(info.url);
    (*headers)[spdy::kHttp2SchemeHeader] = info.url.scheme();
    (*headers)[spdy::kHttp2PathHeader] = info.url.PathForRequest();
  }

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "connection" ||
        name == "proxy-connection" || name == "transfer-encoding" ||
        name == "host") {
      continue;
    }
    AddSpdyHeader(name, it.value(), headers);
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
    AddSpdyHeader(kHttp2PriorityHeader,
                  quic::SerializePriorityFieldValue(quic_priority), headers);
  }
}

void CreateSpdyHeadersFromHttpRequestForWebSocket(
    const GURL& url,
    const HttpRequestHeaders& request_headers,
    spdy::Http2HeaderBlock* headers) {
  (*headers)[spdy::kHttp2MethodHeader] = "CONNECT";
  (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndOptionalPort(url);
  (*headers)[spdy::kHttp2SchemeHeader] = "https";
  (*headers)[spdy::kHttp2PathHeader] = url.PathForRequest();
  (*headers)[spdy::kHttp2ProtocolHeader] = "websocket";

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "upgrade" ||
        name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding" || name == "host") {
      continue;
    }
    AddSpdyHeader(name, it.value(), headers);
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
    const spdy::Http2HeaderBlock& spdy_headers,
    HttpRequestHeaders* http_headers) {
  for (const auto& it : spdy_headers) {
    base::StringPiece key = it.first;
    if (key[0] == ':') {
      key.remove_prefix(1);
    }
    std::vector<base::StringPiece> values = base::SplitStringPiece(
        it.second, "\0", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const auto& value : values) {
      http_headers->SetHeader(key, value);
    }
  }
}

}  // namespace net
