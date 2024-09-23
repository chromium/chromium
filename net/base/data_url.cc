// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include "net/base/data_url.h"

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {

namespace {

// Determine if we are in the deprecated mode of whitespace removal
// Enterprise policies can enable this command line flag to force
// the old (non-standard compliant) behavior.
bool HasRemoveWhitespaceCommandLineFlag() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line) {
    return false;
  }
  return command_line->HasSwitch(kRemoveWhitespaceForDataURLs);
}

// https://infra.spec.whatwg.org/#ascii-whitespace, which is referenced by
// https://infra.spec.whatwg.org/#forgiving-base64, does not include \v in the
// set of ASCII whitespace characters the way Unicode does.
bool IsBase64Whitespace(char c) {
  return c != '\v' && base::IsAsciiWhitespace(c);
}

// A data URL is ready for decode if it:
//   - Doesn't need any extra padding.
//   - Does not have any escaped characters.
//   - Does not have any whitespace.
bool IsDataURLReadyForDecode(std::string_view body) {
  return (body.length() % 4) == 0 && base::ranges::none_of(body, [](char c) {
           return c == '%' || IsBase64Whitespace(c);
         });
}

}  // namespace

bool DataURL::Parse(const GURL& url,
                    std::string* mime_type,
                    std::string* charset,
                    std::string* data) {
  if (!url.is_valid() || !url.has_scheme())
    return false;

  DCHECK(mime_type->empty());
  DCHECK(charset->empty());
  DCHECK(!data || data->empty());

  // Avoid copying the URL content which can be expensive for large URLs.
  std::string_view content = url.GetContentPiece();

  std::string_view::const_iterator comma = base::ranges::find(content, ',');
  if (comma == content.end())
    return false;

  std::vector<std::string_view> meta_data =
      base::SplitStringPiece(base::MakeStringPiece(content.begin(), comma), ";",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // These are moved to |mime_type| and |charset| on success.
  std::string mime_type_value;
  std::string charset_value;
  auto iter = meta_data.cbegin();
  if (iter != meta_data.cend()) {
    mime_type_value = base::ToLowerASCII(*iter);
    ++iter;
  }

  static constexpr std::string_view kBase64Tag("base64");
  static constexpr std::string_view kCharsetTag("charset=");

  bool base64_encoded = false;
  for (; iter != meta_data.cend(); ++iter) {
    if (!base64_encoded &&
        base::EqualsCaseInsensitiveASCII(*iter, kBase64Tag)) {
      base64_encoded = true;
    } else if (charset_value.empty() &&
               base::StartsWith(*iter, kCharsetTag,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      charset_value = std::string(iter->substr(kCharsetTag.size()));
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

    auto raw_body = base::MakeStringPiece(comma + 1, content.end());

    // For base64, we may have url-escaped whitespace which is not part
    // of the data, and should be stripped. Otherwise, the escaped whitespace
    // could be part of the payload, so don't strip it.
    if (base64_encoded) {
      if (base::FeatureList::IsEnabled(features::kOptimizeParsingDataUrls)) {
        // Since whitespace and invalid characters in input will always cause
        // `Base64Decode` to fail, just handle unescaping the URL on failure.
        // This is not much slower than scanning the URL for being well formed
        // first, even for input with whitespace.
        if (!base::Base64Decode(raw_body, data)) {
          std::string unescaped_body =
              base::UnescapeBinaryURLComponent(raw_body);
          if (!base::Base64Decode(unescaped_body, data,
                                  base::Base64DecodePolicy::kForgiving)) {
            return false;
          }
        }
      } else {
        // If the data URL is well formed, we can decode it immediately.
        if (IsDataURLReadyForDecode(raw_body)) {
          if (!base::Base64Decode(raw_body, data)) {
            return false;
          }
        } else {
          std::string unescaped_body =
              base::UnescapeBinaryURLComponent(raw_body);
          if (!base::Base64Decode(unescaped_body, data,
                                  base::Base64DecodePolicy::kForgiving)) {
            return false;
          }
        }
      }
    } else {
      // `temp`'s storage needs to be outside feature check since `raw_body` is
      // a string_view.
      std::string temp;
      // Strip whitespace for non-text MIME types. This is controlled either by
      // the feature (finch kill switch) or an enterprise policy which sets the
      // command line flag.
      if (!base::FeatureList::IsEnabled(features::kKeepWhitespaceForDataUrls) ||
          HasRemoveWhitespaceCommandLineFlag()) {
        if (!(mime_type_value.compare(0, 5, "text/") == 0 ||
              mime_type_value.find("xml") != std::string::npos)) {
          temp = std::string(raw_body);
          std::erase_if(temp, base::IsAsciiWhitespace<char>);
          raw_body = temp;
        }
      }

      *data = base::UnescapeBinaryURLComponent(raw_body);
    }
  }

  *mime_type = std::move(mime_type_value);
  *charset = std::move(charset_value);
  return true;
}

Error DataURL::BuildResponse(const GURL& url,
                             std::string_view method,
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
  if (base::FeatureList::IsEnabled(features::kOptimizeParsingDataUrls)) {
    *headers = HttpResponseHeaders::TryToCreateForDataURL(content_type);
  } else {
    *headers = HttpResponseHeaders::TryToCreate(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type:" +
        content_type);
  }
  // Above line should always succeed - TryToCreate() only fails when there are
  // nulls in the string, and DataURL::Parse() can't return nulls in anything
  // but the |data| argument.
  DCHECK(*headers);

  if (base::EqualsCaseInsensitiveASCII(method, "HEAD"))
    data->clear();

  return OK;
}

}  // namespace net
