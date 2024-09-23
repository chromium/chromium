// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_APPLE_HTTP_RESPONSE_HEADERS_UTIL_H_
#define NET_BASE_APPLE_HTTP_RESPONSE_HEADERS_UTIL_H_

#include "net/base/net_export.h"
#include "net/http/http_response_headers.h"

@class NSHTTPURLResponse;

namespace net {

// Placeholder status description since the actual text from the headers is not
// available.
extern const char kDummyHttpStatusDescription[];

// Tries to decode `string` as if it was a "utf-8" encoded string incorrectly
// decoded as "latin1" encoded string. Returns the original string if it does
// not appear to be a mis-decoded string.
//
// HTTP headers have no encoding, but NSHTTPURLResponse decode them as if they
// were using "latin1" encoding as the rfc recommends (see [1]). Servers do
// sometime sends data in "utf-8" encoding. Interpreting "utf-8" encoded string
// as "latin1" results in garbled characters as soon as the string contains non
// US-ASCII characters (aka mojibake). Hopefully, this incorrect decoding is
// reversible as "latin1" maps each bytes to the same unicode character (i.e.
// byte '\xab' maps to '\u00ab'). See [2] for example of a web server serving
// "utf-8" encoded string incorrectly decoded.
//
// [1]: https://www.rfc-editor.org/rfc/rfc7230#section-3.2.4.
// [2]: https://crbug.com/1333351
NET_EXPORT NSString* FixNSStringIncorrectlyDecodedAsLatin1(NSString* string);

// Constructs a net::HttpResponseHeaders from |response|.
// Note: The HTTP version and the status code description are not accessible
// from NSHTTPURLResponse, so HTTP/1.0 and kDummyHttpStatusDescription will
// be used in the status line instead.
NET_EXPORT scoped_refptr<HttpResponseHeaders>
CreateHeadersFromNSHTTPURLResponse(NSHTTPURLResponse* response);

}  // namespace net

#endif  // NET_BASE_APPLE_HTTP_RESPONSE_HEADERS_UTIL_H_
