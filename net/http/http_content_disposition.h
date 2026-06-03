// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
#define NET_HTTP_HTTP_CONTENT_DISPOSITION_H_

#include <string>
#include <string_view>

#include "net/base/net_export.h"

namespace net {

class HttpResponseHeaders;

class NET_EXPORT HttpContentDisposition {
 public:
  enum Type {
    INLINE,
    ATTACHMENT,
  };

  // Properties of the Content-Disposition header. These flags are used to
  // report download metrics in UMA. This enum isn't directly used in UMA but
  // mapped to another one for binary compatiblity; ie. changes are OK.
  enum ParseResultFlags {
    INVALID = 0,

    // A valid disposition-type is present.
    HAS_DISPOSITION_TYPE = 1 << 0,

    // The disposition-type is not 'inline' or 'attachment'.
    HAS_UNKNOWN_DISPOSITION_TYPE = 1 << 1,

    // Has a valid non-empty 'filename' attribute.
    HAS_FILENAME = 1 << 2,

    // Has a valid non-empty 'filename*' attribute.
    HAS_EXT_FILENAME = 1 << 3,

    // The following fields are properties of the 'filename' attribute:

    // Quoted-string contains non-ASCII characters.
    HAS_NON_ASCII_STRINGS = 1 << 4,

    // Quoted-string contains percent-encoding.
    HAS_PERCENT_ENCODED_STRINGS = 1 << 5,

    // Quoted-string contains RFC 2047 encoded words.
    HAS_RFC2047_ENCODED_STRINGS = 1 << 6,

    // Has a filename that starts with a single quote.
    HAS_SINGLE_QUOTED_FILENAME = 1 << 7,
  };

  // NOTE: Until features::kOnlyParseFirstContentDisposition is removed, new
  // consumers should use EnumerateHeader() to get the first Content-Disposition
  // header and then pass it to the constructor that takes a std::string_view.
  // Once that's removed, however, this constructor should be used. This comment
  // should be removed when that happens.
  // TODO(crbug.com/519218483): Remove this comment in late Q3/Q4 2026, when
  // the feature is removed.
  //
  // Preferred constructor. Uses the first Content-Disposition header in the
  // response (retrieved using `HttpResponseHeaders::EnumerateHeader`). Only
  // looks at the first Content-Disposition header. The HTTP code guarantees
  // that if there are multiple Content-Disposition fields, they're all
  // identical, but that might not be the case if they come from other sources
  // (e.g., data URLs, extensions).
  //
  // Note that no Content-Disposition header or an empty header results in an
  // inline type with no filename.
  HttpContentDisposition(const HttpResponseHeaders& headers,
                         const std::string& referrer_charset);

  // Overload for the case when an HttpResponseHeaders is not available. While
  // both overloads behave the same, please use the first constructor if
  // possible, to avoid having to pull out the right header from
  // HttpResponseHeaders, and for somewhat improved performance. Can be called
  // either on the first header only, or all headers merged together and
  // separated by commas (and will return the same result). Should not be called
  // directly on Content-Disposition headers after the first one, even if the
  // first one is invalid.
  HttpContentDisposition(std::string_view header,
                         const std::string& referrer_charset);

  HttpContentDisposition(const HttpContentDisposition&) = delete;
  HttpContentDisposition& operator=(const HttpContentDisposition&) = delete;

  ~HttpContentDisposition();

  bool is_attachment() const { return type() == ATTACHMENT; }

  Type type() const { return type_; }
  const std::string& filename() const { return filename_; }

  // A combination of ParseResultFlags values.
  int parse_result_flags() const { return parse_result_flags_; }

 private:
  void Parse(std::string_view header, const std::string& referrer_charset);

  // Parses the content disposition type, if present, and sets `type_`. Returns
  // `header` with the content disposition type removed.
  std::string_view ConsumeDispositionType(std::string_view header);

  Type type_ = INLINE;
  std::string filename_;
  int parse_result_flags_ = INVALID;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
