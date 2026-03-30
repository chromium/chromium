// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include "net/base/data_url.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/base64.h"
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

bool IsFurtherOptimizeParsingDataUrlsEnabled() {
  static const bool further_optimize_parsing_enabled =
      base::FeatureList::IsEnabled(features::kFurtherOptimizeParsingDataUrls);
  return further_optimize_parsing_enabled;
}

bool IsDataUrlMimeTypeParameterPreservationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kDataUrlMimeTypeParameterPreservation);
}

constexpr std::string_view kBase64Tag("base64");
constexpr std::string_view kCharsetTag("charset=");

// Parses a charset parameter value starting after "charset=".
// Returns true if the charset is valid, false otherwise.
// On success, populates |charset_value| with the unquoted charset (if
// |charset_value| was empty) and |quoted_charset_value| if the charset
// needs quoting in the final MIME type.
bool ParseCharsetParameter(std::string_view charset_candidate,
                           std::string* charset_value,
                           std::string_view* quoted_charset_value) {
  if (charset_candidate.empty()) {
    return false;  // Skip empty charset values.
  }

  std::string_view charset_to_validate = charset_candidate;

  // Handle quoted charset values like charset="utf-8".
  if (charset_candidate.size() >= 2 && charset_candidate.front() == '"' &&
      charset_candidate.back() == '"') {
    charset_to_validate =
        charset_candidate.substr(1, charset_candidate.size() - 2);

    // Handle quoted charset with leading space (e.g., charset=" foo").
    // Treat it consistently with unquoted leading-space charset (charset= foo)
    // by storing for quoting in output.
    if (!charset_to_validate.empty() && charset_to_validate[0] == ' ') {
      *quoted_charset_value = charset_to_validate;
      return true;
    }

    // The grammar for charset is not specially defined in RFC2045 and
    // RFC2397. It just needs to be a token (after unquoting).
    if (!HttpUtil::IsToken(charset_to_validate)) {
      return false;
    }

    // Use the first valid charset parameter value encountered.
    if (charset_value->empty()) {
      *charset_value = std::string(charset_to_validate);
    }
    return true;
  }

  // Handle charset with leading space - must be quoted in output.
  if (charset_candidate[0] == ' ') {
    // Don't set charset_value since BuildResponse shouldn't append it.
    // Store the value - we build the quoted form during final concatenation.
    *quoted_charset_value = charset_candidate;
    return true;
  }

  // Handle unquoted charset values.
  if (!HttpUtil::IsToken(charset_to_validate)) {
    return false;
  }

  // Use the first valid charset parameter value encountered.
  if (charset_value->empty()) {
    *charset_value = std::string(charset_to_validate);
  }
  return true;
}

// Parameter struct for storing parsed parameters with minimal allocation.
// For malformed quoted values (e.g., a="word), we store the full param
// and set the flag to append a closing quote during concatenation.
struct Parameter {
  std::string_view value;
  bool needs_closing_quote = false;
};

// Parses a non-charset parameter and adds it to the parameters vector.
// Returns true to continue processing, false is not used (always succeeds).
void ParseNonCharsetParameter(std::string_view param,
                              size_t equals_pos,
                              std::vector<Parameter>* parameters) {
  std::string_view param_name = param.substr(0, equals_pos);

  // Skip parameters with trailing space in the name (invalid syntax).
  if (param_name.empty() || param_name.back() == ' ') {
    return;
  }

  std::string_view param_value = param.substr(equals_pos + 1);

  // Handle quoted parameter values that may have been truncated when
  // splitting at the comma separator. For example, in the data URL
  // "data:text/plain;a=",",X", the first comma is inside the quoted
  // value, but we split there anyway (per spec). This results in
  // param_value being just '"' (a single quote character).
  //
  // Per the Fetch Standard, we normalize such incomplete quoted values
  // by appending a closing quote. For example, a="word becomes a="word".
  // A value is considered "properly quoted" if it has at least 2 characters
  // and both starts and ends with a quote.
  bool starts_with_quote = !param_value.empty() && param_value.front() == '"';
  bool ends_with_quote = param_value.size() >= 2 && param_value.back() == '"';

  if (starts_with_quote && !ends_with_quote) {
    // Store the full param and mark for closing quote appending.
    // The normalized form (param + '"') is built during concatenation.
    parameters->emplace_back(param, true);
  } else {
    parameters->emplace_back(param, false);
  }
}

// Appends collected parameters to the MIME type string.
void AppendParametersToMimeType(const std::vector<Parameter>& parameters,
                                std::string_view quoted_charset_value,
                                std::string* mime_type_value) {
  for (const auto& param : parameters) {
    mime_type_value->append(";");
    mime_type_value->append(param.value);
    if (param.needs_closing_quote) {
      // Append closing quote for malformed quoted values (e.g., a="word ->
      // a="word")
      mime_type_value->append("\"");
    }
  }

  // Append quoted charset (e.g., charset with leading space) after other
  // parameters. Build the quoted form directly to avoid allocation.
  if (!quoted_charset_value.empty()) {
    mime_type_value->append(";charset=\"")
        .append(quoted_charset_value)
        .append("\"");
  }
}

// Parses metadata with MIME type parameter preservation (new behavior).
// Preserves all MIME type parameters (e.g., "charset=utf-8", "boundary=xxx")
// in the final MIME type string for WPT compliance. The Fetch Standard
// specifies that data URL MIME type parameters should be preserved in the
// Content-Type header. See: https://fetch.spec.whatwg.org/#data-url-processor
//
// Returns false if parsing fails (e.g., invalid charset), true otherwise.
bool ParseMetadataWithParameterPreservation(
    const std::vector<std::string_view>& meta_data,
    std::string* mime_type_value,
    std::string* charset_value,
    bool* base64_encoded) {
  std::vector<Parameter> parameters;
  std::string_view quoted_charset_value;

  bool mime_type_is_empty = mime_type_value->empty();

  auto iter = meta_data.cbegin();
  if (iter != meta_data.cend()) {
    ++iter;  // Skip the MIME type (already processed).
  }

  // Check if the last parameter is "base64" (case-insensitive), which
  // indicates the body is base64-encoded. Per the Fetch Standard, "base64"
  // must be the last token before the comma separator.
  if (iter != meta_data.cend()) {
    auto last_iter = meta_data.cend() - 1;
    if (base::EqualsCaseInsensitiveASCII(*last_iter, kBase64Tag)) {
      *base64_encoded = true;
    }
  }

  // Iterate through all parameters, excluding the "base64" token if present.
  auto end_iter = *base64_encoded ? (meta_data.cend() - 1) : meta_data.cend();
  for (; iter != end_iter; ++iter) {
    size_t equals_pos = iter->find('=');
    if (equals_pos == std::string_view::npos) {
      continue;
    }

    // Handle charset parameter specially - we need to extract both the value
    // for the |charset| output parameter and preserve it in the MIME type.
    if (base::StartsWith(*iter, kCharsetTag,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      std::string_view charset_candidate = iter->substr(kCharsetTag.size());
      if (!ParseCharsetParameter(charset_candidate, charset_value,
                                 &quoted_charset_value)) {
        if (!charset_candidate.empty()) {
          return false;  // Invalid charset token.
        }
        // Empty charset - just skip it.
      }
    } else {
      ParseNonCharsetParameter(*iter, equals_pos, &parameters);
    }
  }

  // Apply default MIME type and charset fallbacks.
  if (mime_type_is_empty) {
    *mime_type_value = "text/plain";
    // Only add default charset if no explicit charset or other parameters
    // were specified. This ensures "data:;x=y,X" gets "text/plain;x=y"
    // without adding "charset=US-ASCII".
    if (charset_value->empty() && parameters.empty()) {
      if ((meta_data.size() <= 1) ||
          (meta_data.size() == 2 && *base64_encoded)) {
        *charset_value = "US-ASCII";
      }
    }
  } else if (!ParseMimeTypeWithoutParameter(*mime_type_value, nullptr,
                                            nullptr)) {
    // Fallback to the default as recommended in RFC2045 when the mediatype
    // value is invalid. For this case, we don't respect |charset| but force
    // it set to "US-ASCII". Note: base64_encoded is intentionally preserved
    // here. Per the Fetch Standard data: URL processor [1], base64 detection
    // (step 11) happens before MIME type validation (step 13-14), so an invalid
    // MIME type should not prevent base64 body decoding. This matches the
    // legacy behavior and preserves backward compatibility with data URLs
    // like "data:image;base64,..." or "data:image/image/jpeg;base64,...".
    // [1] https://fetch.spec.whatwg.org/#data-url-processor
    *mime_type_value = "text/plain";
    *charset_value = "US-ASCII";
    parameters.clear();
  }

  AppendParametersToMimeType(parameters, quoted_charset_value, mime_type_value);
  return true;
}

// Parses metadata using the legacy behavior (original implementation).
// Only extracts charset, does not preserve other parameters.
//
// Returns false if parsing fails (e.g., invalid charset), true otherwise.
bool ParseMetadataLegacy(const std::vector<std::string_view>& meta_data,
                         std::string* mime_type_value,
                         std::string* charset_value,
                         bool* base64_encoded) {
  auto iter = meta_data.cbegin();
  if (iter != meta_data.cend()) {
    ++iter;  // Skip the MIME type (already processed).
  }

  for (; iter != meta_data.cend(); ++iter) {
    if (!*base64_encoded &&
        base::EqualsCaseInsensitiveASCII(*iter, kBase64Tag)) {
      *base64_encoded = true;
    } else if (charset_value->empty() &&
               base::StartsWith(*iter, kCharsetTag,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      *charset_value = std::string(iter->substr(kCharsetTag.size()));
      // The grammar for charset is not specially defined in RFC2045 and
      // RFC2397. It just needs to be a token.
      if (!HttpUtil::IsToken(*charset_value)) {
        return false;
      }
    }
  }

  if (mime_type_value->empty()) {
    // Fallback to the default if nothing specified in the mediatype part as
    // specified in RFC2045. As specified in RFC2397, we use |charset| even if
    // |mime_type| is empty.
    *mime_type_value = "text/plain";
    if (charset_value->empty()) {
      *charset_value = "US-ASCII";
    }
  } else if (!ParseMimeTypeWithoutParameter(*mime_type_value, nullptr,
                                            nullptr)) {
    // Fallback to the default as recommended in RFC2045 when the mediatype
    // value is invalid. For this case, we don't respect |charset| but force
    // it set to "US-ASCII".
    *mime_type_value = "text/plain";
    *charset_value = "US-ASCII";
  }

  return true;
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

  std::optional<std::pair<std::string_view, std::string_view>>
      media_type_and_body = base::SplitStringOnce(content, ',');
  if (!media_type_and_body) {
    return false;
  }

  std::vector<std::string_view> meta_data =
      base::SplitStringPiece(media_type_and_body->first, ";",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // These are moved to |mime_type| and |charset| on success.
  std::string mime_type_value;
  std::string charset_value;

  if (!meta_data.empty()) {
    mime_type_value = base::ToLowerASCII(meta_data[0]);
  }

  bool base64_encoded = false;

  // Parse metadata using either the new parameter-preserving behavior or
  // the legacy behavior, depending on the feature flag.
  if (IsDataUrlMimeTypeParameterPreservationEnabled()) {
    if (!ParseMetadataWithParameterPreservation(
            meta_data, &mime_type_value, &charset_value, &base64_encoded)) {
      return false;
    }
  } else {
    if (!ParseMetadataLegacy(meta_data, &mime_type_value, &charset_value,
                             &base64_encoded)) {
      return false;
    }
  }

  // The caller may not be interested in receiving the data.
  if (data) {
    std::string_view raw_body = media_type_and_body->second;

    // For base64, we may have url-escaped whitespace which is not part
    // of the data, and should be stripped. Otherwise, the escaped whitespace
    // could be part of the payload, so don't strip it.
    if (base64_encoded) {
      if (base::FeatureList::IsEnabled(features::kSimdutfBase64Support)) {
        if (IsFurtherOptimizeParsingDataUrlsEnabled()) {
          // Based on https://fetch.spec.whatwg.org/#data-url-processor, we can
          // always use forgiving-base64 decode.
          // Forgiving-base64 decode consists of 2 passes: removing all ASCII
          // whitespace, then base64 decoding. For data URLs, it consists of 3
          // passes: percent-decoding, removing all ASCII whitespace, then
          // base64 decoding. To do this with as few passes as possible, we try
          // base64 decoding without any modifications in the "happy path". If
          // that fails, we percent-decode, then try the base64 decode again.
          if (!SimdutfBase64Decode(raw_body, data,
                                   base::Base64DecodePolicy::kForgiving)) {
            std::string unescaped_body =
                base::UnescapeBinaryURLComponent(raw_body);
            if (!SimdutfBase64Decode(unescaped_body, data,
                                     base::Base64DecodePolicy::kForgiving)) {
              return false;
            }
          }
        } else {
          // Since whitespace and invalid characters in input will always cause
          // `Base64Decode` to fail, just handle unescaping the URL on failure.
          // This is not much slower than scanning the URL for being well formed
          // first, even for input with whitespace.
          if (!SimdutfBase64Decode(raw_body, data)) {
            std::string unescaped_body =
                base::UnescapeBinaryURLComponent(raw_body);
            if (!SimdutfBase64Decode(unescaped_body, data,
                                     base::Base64DecodePolicy::kForgiving)) {
              return false;
            }
          }
        }
      } else {
        if (IsFurtherOptimizeParsingDataUrlsEnabled()) {
          // Based on https://fetch.spec.whatwg.org/#data-url-processor, we can
          // always use forgiving-base64 decode.
          // Forgiving-base64 decode consists of 2 passes: removing all ASCII
          // whitespace, then base64 decoding. For data URLs, it consists of 3
          // passes: percent-decoding, removing all ASCII whitespace, then
          // base64 decoding. To do this with as few passes as possible, we try
          // base64 decoding without any modifications in the "happy path". If
          // that fails, we percent-decode, then try the base64 decode again.
          if (!base::Base64Decode(raw_body, data,
                                  base::Base64DecodePolicy::kForgiving)) {
            std::string unescaped_body =
                base::UnescapeBinaryURLComponent(raw_body);
            if (!base::Base64Decode(unescaped_body, data,
                                    base::Base64DecodePolicy::kForgiving)) {
              return false;
            }
          }
        } else {
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
        }
      }
    } else {
      // `temp`'s storage needs to be outside feature check since `raw_body` is
      // a string_view.
      std::string temp;
      // Strip whitespace for non-text MIME types if there's a command line flag
      // indicating this needs to be done. The flag may be set by an enterprise
      // policy.
      if (HasRemoveWhitespaceCommandLineFlag()) {
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

  std::string parsed_mime_type;
  if (!DataURL::Parse(url, &parsed_mime_type, charset, data)) {
    return ERR_INVALID_URL;
  }

  DCHECK(!parsed_mime_type.empty());

  // "charset" in the Content-Type header is specified explicitly to follow
  // the "token" ABNF in the HTTP spec. When the DataURL::Parse() call is
  // successful, it's guaranteed that the string in |charset| follows the
  // "token" ABNF.
  std::string content_type = parsed_mime_type;
  if (!charset->empty())
    content_type.append(";charset=" + *charset);
  // The terminal double CRLF isn't needed by TryToCreateForDataURL().
  *headers = HttpResponseHeaders::TryToCreateForDataURL(content_type);
  // Above line should always succeed - TryToCreateForDataURL() only fails when
  // there are nulls in the string, and DataURL::Parse() can't return nulls in
  // anything but the |data| argument.
  DCHECK(*headers);

  // Return only the MIME type essence without parameters for compatibility
  // with callers that expect token/token in the MIME type field.
  std::optional<std::string> mime_type_without_parameters =
      ExtractMimeTypeFromMediaType(parsed_mime_type,
                                   /*accept_comma_separated=*/false);
  if (!mime_type_without_parameters) {
    return ERR_INVALID_URL;
  }
  *mime_type = std::move(*mime_type_without_parameters);

  if (base::EqualsCaseInsensitiveASCII(method, "HEAD"))
    data->clear();

  return OK;
}

}  // namespace net
