/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/public/platform/web_cors.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/url_util.h"

using network::mojom::CORSError;

namespace blink {

namespace WebCORS {

namespace {

// A parser for the value of the Access-Control-Expose-Headers header.
class HTTPHeaderNameListParser {
  STACK_ALLOCATED();

 public:
  explicit HTTPHeaderNameListParser(const String& value)
      : value_(value), pos_(0) {}

  // Tries parsing |value_| expecting it to be conforming to the #field-name
  // ABNF rule defined in RFC 7230. Returns with the field-name entries stored
  // in |output| when successful. Otherwise, returns with |output| kept empty.
  //
  // |output| must be empty.
  void Parse(WebHTTPHeaderSet& output) {
    DCHECK(output.empty());

    while (true) {
      ConsumeSpaces();

      size_t token_start = pos_;
      ConsumeTokenChars();
      size_t token_size = pos_ - token_start;
      if (token_size == 0) {
        output.clear();
        return;
      }

      const CString& name = value_.Substring(token_start, token_size).Ascii();
      output.emplace(name.data(), name.length());

      ConsumeSpaces();

      if (pos_ == value_.length())
        return;

      if (value_[pos_] == ',') {
        ++pos_;
      } else {
        output.clear();
        return;
      }
    }
  }

 private:
  // Consumes zero or more spaces (SP and HTAB) from value_.
  void ConsumeSpaces() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c != ' ' && c != '\t')
        return;
      ++pos_;
    }
  }

  // Consumes zero or more tchars from value_.
  void ConsumeTokenChars() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c > 0x7F || !net::HttpUtil::IsTokenChar(c))
        return;
      ++pos_;
    }
  }

  const String value_;
  size_t pos_;
};

}  // namespace

WebHTTPHeaderSet ExtractCorsExposedHeaderNamesList(
    network::mojom::FetchCredentialsMode credentials_mode,
    const WebURLResponse& response) {
  // If a response was fetched via a service worker, it will always have
  // CorsExposedHeaderNames set from the Access-Control-Expose-Headers header.
  // For requests that didn't come from a service worker, just parse the CORS
  // header.
  if (response.WasFetchedViaServiceWorker()) {
    WebHTTPHeaderSet header_set;
    for (const auto& header : response.CorsExposedHeaderNames())
      header_set.emplace(header.Ascii().data(), header.Ascii().length());
    return header_set;
  }

  WebHTTPHeaderSet header_set;
  HTTPHeaderNameListParser parser(response.HttpHeaderField(
      WebString(HTTPNames::Access_Control_Expose_Headers)));
  parser.Parse(header_set);

  if (credentials_mode != network::mojom::FetchCredentialsMode::kInclude &&
      header_set.find("*") != header_set.end()) {
    header_set.clear();
    for (const auto& header :
         response.ToResourceResponse().HttpHeaderFields()) {
      CString name = header.key.Ascii();
      header_set.emplace(name.data(), name.length());
    }
  }
  return header_set;
}

bool IsOnAccessControlResponseHeaderWhitelist(const WebString& name) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      WebHTTPHeaderSet, allowed_cross_origin_response_headers,
      ({
          "cache-control", "content-language", "content-type", "expires",
          "last-modified", "pragma",
      }));
  return allowed_cross_origin_response_headers.find(name.Ascii().data()) !=
         allowed_cross_origin_response_headers.end();
}

// In the spec, https://fetch.spec.whatwg.org/#ref-for-concept-request-mode,
// No-CORS mode is highly discouraged from using it for new features. Only
// legacy usages for backward compatibility are allowed except for well-designed
// usages over the fetch API.
bool IsNoCORSAllowedContext(mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::AUDIO:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::OBJECT:
    case mojom::RequestContextType::PLUGIN:
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::SHARED_WORKER:
    case mojom::RequestContextType::VIDEO:
    case mojom::RequestContextType::WORKER:
      return true;
    default:
      return false;
  }
}

}  // namespace WebCORS

}  // namespace blink
