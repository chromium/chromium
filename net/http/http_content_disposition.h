// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
#define NET_HTTP_HTTP_CONTENT_DISPOSITION_H_

#include <string>

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {

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

  HttpContentDisposition(const std::string& header,
                         const std::string& referrer_charset);
  ~HttpContentDisposition();

  bool is_attachment() const { return type() == ATTACHMENT; }

  Type type() const { return type_; }
  const std::string& filename() const { return filename_; }

  // A combination of ParseResultFlags values.
  int parse_result_flags() const { return parse_result_flags_; }

 private:
  void Parse(const std::string& header, const std::string& referrer_charset);
  std::string::const_iterator ConsumeDispositionType(
      std::string::const_iterator begin, std::string::const_iterator end);

  Type type_;
  std::string filename_;
  int parse_result_flags_;

  DISALLOW_COPY_AND_ASSIGN(HttpContentDisposition);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CONTENT_DISPOSITION_H_
