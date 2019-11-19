// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"

namespace network {

namespace {

// Headers that consumers are not trusted to set. All "Proxy-" prefixed messages
// are blocked inline. The"Authorization" auth header is deliberately not
// included, since OAuth requires websites be able to set it directly. These are
// a subset of headers forbidden by the fetch spec.
//
// This list has some values in common with
// https://fetch.spec.whatwg.org/#forbidden-header-name, but excludes some
// values that are still set by the caller in Chrome.
const char* kUnsafeHeaders[] = {
    // This is determined by the upload body and set by net/. A consumer
    // overriding that could allow for Bad Things.
    net::HttpRequestHeaders::kContentLength,

    // Disallow setting the Host header because it can conflict with specified
    // URL and logic related to isolating sites.
    net::HttpRequestHeaders::kHost,

    // Trailers are not supported.
    "Trailer", "Te",

    // Websockets use a different API.
    "Upgrade",

    // Obsolete header, and network stack manages headers itself.
    "Cookie2",

    // Not supported by net/.
    "Keep-Alive",

    // Forbidden by the fetch spec.
    net::HttpRequestHeaders::kTransferEncoding,

    // TODO(mmenke): Figure out what to do about the remaining headers:
    // Connection, Cookie, Date, Expect, Referer, Via.
};

// Headers that consumers are currently allowed to set, with the exception of
// certain values could cause problems.
// TODO(mmenke): Gather stats on these, and see if these headers can be banned
// outright instead.
const struct {
  const char* name;
  const char* value;
} kUnsafeHeaderValues[] = {
    // Websockets use a different API.
    {net::HttpRequestHeaders::kConnection, "Upgrade"},
};

}  // namespace

bool IsRequestHeaderSafe(const base::StringPiece& key,
                         const base::StringPiece& value) {
  for (const auto* header : kUnsafeHeaders) {
    if (base::EqualsCaseInsensitiveASCII(header, key))
      return false;
  }

  for (const auto& header : kUnsafeHeaderValues) {
    if (base::EqualsCaseInsensitiveASCII(header.name, key) &&
        base::EqualsCaseInsensitiveASCII(header.value, value)) {
      return false;
    }
  }

  // Proxy headers are destined for the proxy, so shouldn't be set by callers.
  if (base::StartsWith(key, "Proxy-", base::CompareCase::INSENSITIVE_ASCII))
    return false;

  return true;
}

bool AreRequestHeadersSafe(const net::HttpRequestHeaders& request_headers) {
  net::HttpRequestHeaders::Iterator it(request_headers);

  while (it.GetNext()) {
    if (!IsRequestHeaderSafe(it.name(), it.value()))
      return false;
  }

  return true;
}

}  // namespace network
