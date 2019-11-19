// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include <algorithm>

#include "net/base/data_url.h"

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {

bool DataURL::Parse(const GURL& url,
                    std::string* mime_type,
                    std::string* charset,
                    std::string* data) {
  if (!url.is_valid() || !url.has_scheme())
    return false;

  DCHECK(mime_type->empty());
  DCHECK(charset->empty());
  DCHECK(!data || data->empty());

  std::string content = url.GetContent();

  std::string::const_iterator begin = content.begin();
  std::string::const_iterator end = content.end();

  std::string::const_iterator comma = std::find(begin, end, ',');

  if (comma == end)
    return false;

  std::vector<base::StringPiece> meta_data =
      base::SplitStringPiece(base::StringPiece(begin, comma), ";",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // These are moved to |mime_type| and |charset| on success.
  std::string mime_type_value;
  std::string charset_value;
  auto iter = meta_data.cbegin();
  if (iter != meta_data.cend()) {
    mime_type_value = base::ToLowerASCII(*iter);
    ++iter;
  }

  static constexpr base::StringPiece kBase64Tag("base64");
  static constexpr base::StringPiece kCharsetTag("charset=");

  bool base64_encoded = false;
  for (; iter != meta_data.cend(); ++iter) {
    if (!base64_encoded &&
        base::EqualsCaseInsensitiveASCII(*iter, kBase64Tag)) {
      base64_encoded = true;
    } else if (charset_value.empty() &&
               base::StartsWith(*iter, kCharsetTag,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      charset_value = iter->substr(kCharsetTag.size()).as_string();
      // The grammar for charset is not specially defined in RFC2045 and
      // RFC2397. It just needs to be a token.
      if (!HttpUtil::IsToken(charset_value))
        return false;
    }
  }

  if (mime_type_value.empty()) {
    // Fallback to the default if nothing specified in the mediatype part as
    // specified in RFC2045. As specified in RFC2397, we use |charset| even if
    // |mime_type| is empty.
    mime_type_value = "text/plain";
    if (charset_value.empty())
      charset_value = "US-ASCII";
  } else if (!ParseMimeTypeWithoutParameter(mime_type_value, nullptr,
                                            nullptr)) {
    // Fallback to the default as recommended in RFC2045 when the mediatype
    // value is invalid. For this case, we don't respect |charset| but force it
    // set to "US-ASCII".
    mime_type_value = "text/plain";
    charset_value = "US-ASCII";
  }

  // The caller may not be interested in receiving the data.
  if (data) {
    // Preserve spaces if dealing with text or xml input, same as mozilla:
    //   https://bugzilla.mozilla.org/show_bug.cgi?id=138052
    // but strip them otherwise:
    //   https://bugzilla.mozilla.org/show_bug.cgi?id=37200
    // (Spaces in a data URL should be escaped, which is handled below, so any
    // spaces now are wrong. People expect to be able to enter them in the URL
    // bar for text, and it can't hurt, so we allow it.)
    //
    // TODO(mmenke): Is removing all spaces reasonable? GURL removes trailing
    // spaces itself, anyways. Should we just trim leading spaces instead?
    // Allowing random intermediary spaces seems unnecessary.

    base::StringPiece raw_body(comma + 1, end);

    // For base64, we may have url-escaped whitespace which is not part
    // of the data, and should be stripped. Otherwise, the escaped whitespace
    // could be part of the payload, so don't strip it.
    if (base64_encoded) {
      std::string unescaped_body = UnescapeBinaryURLComponent(raw_body);

      // Strip spaces, which aren't allowed in Base64 encoding.
      base::EraseIf(unescaped_body, base::IsAsciiWhitespace<char>);

      size_t length = unescaped_body.length();
      size_t padding_needed = 4 - (length % 4);
      // If the input wasn't padded, then we pad it as necessary until we have a
      // length that is a multiple of 4 as required by our decoder. We don't
      // correct if the input was incorrectly padded. If |padding_needed| == 3,
      // then the input isn't well formed and decoding will fail with or without
      // padding.
      if ((padding_needed == 1 || padding_needed == 2) &&
          unescaped_body[length - 1] != '=') {
        unescaped_body.resize(length + padding_needed, '=');
      }
      if (!base::Base64Decode(unescaped_body, data))
        return false;
    } else {
      // Strip whitespace for non-text MIME types.
      std::string temp;
      if (!(mime_type_value.compare(0, 5, "text/") == 0 ||
            mime_type_value.find("xml") != std::string::npos)) {
        temp = raw_body.as_string();
        base::EraseIf(temp, base::IsAsciiWhitespace<char>);
        raw_body = temp;
      }

      *data = UnescapeBinaryURLComponent(raw_body);
    }
  }

  *mime_type = std::move(mime_type_value);
  *charset = std::move(charset_value);
  return true;
}

Error DataURL::BuildResponse(const GURL& url,
                             base::StringPiece method,
                             std::string* mime_type,
                             std::string* charset,
                             std::string* data,
                             scoped_refptr<HttpResponseHeaders>* headers) {
  DCHECK(data);
  DCHECK(!*headers);

  if (!DataURL::Parse(url, mime_type, charset, data))
    return ERR_INVALID_URL;

  // |mime_type| set by DataURL::Parse() is guaranteed to be in
  //     token "/" token
  // form. |charset| can be an empty string.
  DCHECK(!mime_type->empty());

  // "charset" in the Content-Type header is specified explicitly to follow
  // the "token" ABNF in the HTTP spec. When the DataURL::Parse() call is
  // successful, it's guaranteed that the string in |charset| follows the
  // "token" ABNF.
  std::string content_type = *mime_type;
  if (!charset->empty())
    content_type.append(";charset=" + *charset);
  // The terminal double CRLF isn't needed by TryToCreate().
  *headers = HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type:" +
      content_type);
  // Above line should always succeed - TryToCreate() only fails when there are
  // nulls in the string, and DataURL::Parse() can't return nulls in anything
  // but the |data| argument.
  DCHECK(*headers);

  if (base::EqualsCaseInsensitiveASCII(method, "HEAD"))
    data->clear();

  return OK;
}

}  // namespace net
