// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DATA_URL_H_
#define NET_BASE_DATA_URL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class HttpResponseHeaders;

// See RFC 2397 for a complete description of the 'data' URL scheme.
//
// Briefly, a 'data' URL has the form:
//
//   data:[<mediatype>][;base64],<data>
//
// The <mediatype> is an Internet media type specification (with optional
// parameters.)  The appearance of ";base64" means that the data is encoded as
// base64.  Without ";base64", the data (as a sequence of octets) is represented
// using ASCII encoding for octets inside the range of safe URL characters and
// using the standard %xx hex encoding of URLs for octets outside that range.
// If <mediatype> is omitted, it defaults to text/plain;charset=US-ASCII.  As a
// shorthand, "text/plain" can be omitted but the charset parameter supplied.
//
class NET_EXPORT DataURL {
 public:
  // This method can be used to parse a 'data' URL into its component pieces.
  //
  // |mime_type| and |charset| must be non-null and point to empty strings.
  //
  // If |data| is null, then the <data> section will not be parsed or validated.
  // If non-null, it must point to an empty string.
  //
  // The resulting mime_type is normalized to lowercase.  The data is the
  // decoded data (e.g.., if the data URL specifies base64 encoding, then the
  // returned data is base64 decoded, and any %-escaped bytes are unescaped).
  //
  // If the media type value doesn't match the media-type production defined in
  // RFC 7231, mime_type will be set to the default value "text/plain". We
  // don't simply fail for this grammar violation since Chromium had been
  // accepting such invalid values. For example, <img> element with the src
  // attribute set to a data URL with an invalid media type "image" (without a
  // slash and subtype) had been displayed. However, the value this method will
  // store in mime_type argument can be used for generating other headers, etc.
  // This could lead to security vulnerability. We don't want to accept
  // arbitrary value and ask each caller to validate the return value.
  //
  // If the charset parameter is specified but its value doesn't match the
  // token production defined in RFC 7230, this method simply fails and returns
  // false.
  //
  // If there's any other grammar violation in the URL, then this method will
  // return false, and all passed in pointers will be unmodified. On success,
  // true is returned.
  static bool Parse(const GURL& url,
                    std::string* mime_type,
                    std::string* charset,
                    std::string* data) WARN_UNUSED_RESULT;

  // Similar to parse, except that it also generates a bogus set of response
  // headers, with Content-Type populated, and takes a method. Only the "HEAD"
  // method modifies the response, resulting in a 0-length body. All arguments
  // except must be non-null. All std::string pointers must point to empty
  // strings, and |*headers| must be nullptr. Returns net::OK on success.
  static Error BuildResponse(const GURL& url,
                             base::StringPiece method,
                             std::string* mime_type,
                             std::string* charset,
                             std::string* data,
                             scoped_refptr<HttpResponseHeaders>* headers)
      WARN_UNUSED_RESULT;
};

}  // namespace net

#endif  // NET_BASE_DATA_URL_H_
