// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_HEADERS_TEST_UTIL_H_
#define NET_HTTP_HTTP_RESPONSE_HEADERS_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace net {

class HttpResponseHeaders;

namespace test {

// Returns a simple text serialization of the HttpResponseHeaders object
// `parsed`. This is used by tests to verify that the object matches an
// expectation string.
//
//  * One line per header, written as:
//        HEADER_NAME: HEADER_VALUE\n
//  * The original case of header names is preserved.
//  * Whitespace around head names/values is stripped.
//  * Repeated headers are not aggregated.
//  * Headers are listed in their original order.
std::string HttpResponseHeadersToSimpleString(
    const scoped_refptr<HttpResponseHeaders>& parsed);

}  // namespace test

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_HEADERS_TEST_UTIL_H_
