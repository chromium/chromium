// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/mime_util.h"
#include "net/base/parse_number.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"

namespace net {

namespace {

template <typename ConstIterator>
void TrimLWSImplementation(ConstIterator* begin, ConstIterator* end) {
  // leading whitespace
  while (*begin < *end && HttpUtil::IsLWS((*begin)[0]))
    ++(*begin);

  // trailing whitespace
  while (*begin < *end && HttpUtil::IsLWS((*end)[-1]))
    --(*end);
}

// Helper class that builds the list of languages for the Accept-Language
// headers.
// The output is a comma-separated list of languages as string.
// Duplicates are removed.
class AcceptLanguageBuilder {
 public:
  // Adds a language to the string.
  // Duplicates are ignored.
  void AddLanguageCode(const std::string& language) {
    // No Q score supported, only supports ASCII.
    DCHECK_EQ(std::string::npos, language.find_first_of("; "));
    DCHECK(base::IsStringASCII(language));
    if (seen_.find(language) == seen_.end()) {
      if (str_.empty()) {
        base::StringAppendF(&str_, "%s", language.c_str());
      } else {
        base::StringAppendF(&str_, ",%s", language.c_str());
      }
      seen_.insert(language);
    }
  }

  // Returns the string constructed up to this point.
  std::string GetString() const { return str_; }

 private:
  // The string that contains the list of languages, comma-separated.
  std::string str_;
  // Set the remove duplicates.
  std::unordered_set<std::string> seen_;
};

// Extract the base language code from a language code.
// If there is no '-' in the code, the original code is returned.
std::string GetBaseLanguageCode(const std::string& language_code) {
  const std::vector<std::string> tokens = base::SplitString(
      language_code, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return tokens.empty() ? "" : tokens[0];
}

}  // namespace

// HttpUtil -------------------------------------------------------------------

std::string HttpUtil::GenerateRequestLine(std::string_view method,
                                          const GURL& url,
                                          bool is_for_get_to_http_proxy) {
  static constexpr char kSuffix[] = " HTTP/1.1\r\n";
  const std::string path = is_for_get_to_http_proxy
                               ? HttpUtil::SpecForRequest(url)
                               : url.PathForRequest();
  return base::StrCat({method, " ", path, kSuffix});
}

// static
std::string HttpUtil::SpecForRequest(const GURL& url) {
  DCHECK(url.is_valid() &&
         (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS()));
  return SimplifyUrlForRequest(url).spec();
}

// static
void HttpUtil::ParseContentType(std::string_view content_type_str,
                                std::string* mime_type,
                                std::string* charset,
                                bool* had_charset,
                                std::string* boundary) {
  std::string mime_type_value;
  base::StringPairs params;
  bool result = ParseMimeType(content_type_str, &mime_type_value, &params);
  // If the server sent "*/*", it is meaningless, so do not store it.
  // Also, reject a mime-type if it does not include a slash.
  // Some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.
  if (!result || content_type_str == "*/*")
    return;

  std::string charset_value;
  bool type_has_charset = false;
  bool type_has_boundary = false;
  for (const auto& param : params) {
    // Trim LWS from param value, ParseMimeType() leaves WS for quoted-string.
    // TODO(mmenke): Check that name has only valid characters.
    if (!type_has_charset &&
        base::EqualsCaseInsensitiveASCII(param.first, "charset")) {
      type_has_charset = true;
      charset_value = std::string(HttpUtil::TrimLWS(param.second));
      continue;
    }

    if (boundary && !type_has_boundary &&
        base::EqualsCaseInsensitiveASCII(param.first, "boundary")) {
      type_has_boundary = true;
      *boundary = std::string(HttpUtil::TrimLWS(param.second));
      continue;
    }
  }

  // If `mime_type_value` is the same as `mime_type`, then just update
  // `charset`. However, if `charset` is empty and `mime_type` hasn't changed,
  // then don't wipe-out an existing `charset`.
  bool eq = base::EqualsCaseInsensitiveASCII(mime_type_value, *mime_type);
  if (!eq) {
    *mime_type = base::ToLowerASCII(mime_type_value);
  }
  if ((!eq && *had_charset) || type_has_charset) {
    *had_charset = true;
    *charset = base::ToLowerASCII(charset_value);
  }
}

// static
bool HttpUtil::ParseRangeHeader(const std::string& ranges_specifier,
                                std::vector<HttpByteRange>* ranges) {
  size_t equal_char_offset = ranges_specifier.find('=');
  if (equal_char_offset == std::string::npos)
    return false;

  // Try to extract bytes-unit part.
  std::string_view bytes_unit =
      std::string_view(ranges_specifier).substr(0, equal_char_offset);

  // "bytes" unit identifier is not found.
  bytes_unit = TrimLWS(bytes_unit);
  if (!base::EqualsCaseInsensitiveASCII(bytes_unit, "bytes")) {
    return false;
  }

  std::string::const_iterator byte_range_set_begin =
      ranges_specifier.begin() + equal_char_offset + 1;
  std::string::const_iterator byte_range_set_end = ranges_specifier.end();

  ValuesIterator byte_range_set_iterator(
      std::string_view(byte_range_set_begin, byte_range_set_end),
      /*delimiter=*/',');
  while (byte_range_set_iterator.GetNext()) {
    std::string_view value = byte_range_set_iterator.value();
    size_t minus_char_offset = value.find('-');
    // If '-' character is not found, reports failure.
    if (minus_char_offset == std::string::npos)
      return false;

    std::string_view first_byte_pos = value.substr(0, minus_char_offset);
    first_byte_pos = TrimLWS(first_byte_pos);

    HttpByteRange range;
    // Try to obtain first-byte-pos.
    if (!first_byte_pos.empty()) {
      int64_t first_byte_position = -1;
      if (!base::StringToInt64(first_byte_pos, &first_byte_position))
        return false;
      range.set_first_byte_position(first_byte_position);
    }

    std::string_view last_byte_pos = value.substr(minus_char_offset + 1);
    last_byte_pos = TrimLWS(last_byte_pos);

    // We have last-byte-pos or suffix-byte-range-spec in this case.
    if (!last_byte_pos.empty()) {
      int64_t last_byte_position;
      if (!base::StringToInt64(last_byte_pos, &last_byte_position))
        return false;
      if (range.HasFirstBytePosition())
        range.set_last_byte_position(last_byte_position);
      else
        range.set_suffix_length(last_byte_position);
    } else if (!range.HasFirstBytePosition()) {
      return false;
    }

    // Do a final check on the HttpByteRange object.
    if (!range.IsValid())
      return false;
    ranges->push_back(range);
  }
  return !ranges->empty();
}

// static
// From RFC 2616 14.16:
// content-range-spec =
//     bytes-unit SP byte-range-resp-spec "/" ( instance-length | "*" )
// byte-range-resp-spec = (first-byte-pos "-" last-byte-pos) | "*"
// instance-length = 1*DIGIT
// bytes-unit = "bytes"
bool HttpUtil::ParseContentRangeHeaderFor206(
    std::string_view content_range_spec,
    int64_t* first_byte_position,
    int64_t* last_byte_position,
    int64_t* instance_length) {
  *first_byte_position = *last_byte_position = *instance_length = -1;
  content_range_spec = TrimLWS(content_range_spec);

  size_t space_position = content_range_spec.find(' ');
  if (space_position == std::string_view::npos) {
    return false;
  }

  // Invalid header if it doesn't contain "bytes-unit".
  if (!base::EqualsCaseInsensitiveASCII(
          TrimLWS(content_range_spec.substr(0, space_position)), "bytes")) {
    return false;
  }

  size_t minus_position = content_range_spec.find('-', space_position + 1);
  if (minus_position == std::string_view::npos) {
    return false;
  }
  size_t slash_position = content_range_spec.find('/', minus_position + 1);
  if (slash_position == std::string_view::npos) {
    return false;
  }

  if (base::StringToInt64(
          TrimLWS(content_range_spec.substr(
              space_position + 1, minus_position - (space_position + 1))),
          first_byte_position) &&
      *first_byte_position >= 0 &&
      base::StringToInt64(
          TrimLWS(content_range_spec.substr(
              minus_position + 1, slash_position - (minus_position + 1))),
          last_byte_position) &&
      *last_byte_position >= *first_byte_position &&
      base::StringToInt64(
          TrimLWS(content_range_spec.substr(slash_position + 1)),
          instance_length) &&
      *instance_length > *last_byte_position) {
    return true;
  }
  *first_byte_position = *last_byte_position = *instance_length = -1;
  return false;
}

// static
bool HttpUtil::ParseRetryAfterHeader(const std::string& retry_after_string,
                                     base::Time now,
                                     base::TimeDelta* retry_after) {
  uint32_t seconds;
  base::Time time;
  base::TimeDelta interval;

  if (ParseUint32(retry_after_string, ParseIntFormat::NON_NEGATIVE, &seconds)) {
    interval = base::Seconds(seconds);
  } else if (base::Time::FromUTCString(retry_after_string.c_str(), &time)) {
    interval = time - now;
  } else {
    return false;
  }

  if (interval < base::Seconds(0))
    return false;

  *retry_after = interval;
  return true;
}

// static
std::string HttpUtil::TimeFormatHTTP(base::Time time) {
  static constexpr char kWeekdayName[7][4] = {"Sun", "Mon", "Tue", "Wed",
                                              "Thu", "Fri", "Sat"};
  static constexpr char kMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr",
                                             "May", "Jun", "Jul", "Aug",
                                             "Sep", "Oct", "Nov", "Dec"};
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf(
      "%s, %02d %s %04d %02d:%02d:%02d GMT", kWeekdayName[exploded.day_of_week],
      exploded.day_of_month, kMonthName[exploded.month - 1], exploded.year,
      exploded.hour, exploded.minute, exploded.second);
}

namespace {

// A header string containing any of the following fields will cause
// an error. The list comes from the fetch standard.
const char* const kForbiddenHeaderFields[] = {
    "accept-charset",
    "accept-encoding",
    "access-control-request-headers",
    "access-control-request-method",
    "access-control-request-private-network",
    "connection",
    "content-length",
    "cookie",
    "cookie2",
    "date",
    "dnt",
    "expect",
    "host",
    "keep-alive",
    "origin",
    "referer",
    "set-cookie",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    // TODO(mmenke): This is no longer banned, but still here due to issues
    // mentioned in https://crbug.com/571722.
    "user-agent",
    "via",
};

// A header string containing any of the following fields with a forbidden
// method name in the value will cause an error. The list comes from the fetch
// standard.
const char* const kForbiddenHeaderFieldsWithForbiddenMethod[] = {
    "x-http-method",
    "x-http-method-override",
    "x-method-override",
};

// The forbidden method names that is defined in the fetch standard, and used
// to check the kForbiddenHeaderFileWithForbiddenMethod above.
const char* const kForbiddenMethods[] = {
    "connect",
    "trace",
    "track",
};

}  // namespace

// static
bool HttpUtil::IsMethodSafe(std::string_view method) {
  return method == "GET" || method == "HEAD" || method == "OPTIONS" ||
         method == "TRACE";
}

// static
bool HttpUtil::IsMethodIdempotent(std::string_view method) {
  return IsMethodSafe(method) || method == "PUT" || method == "DELETE";
}

// static
bool HttpUtil::IsSafeHeader(std::string_view name, std::string_view value) {
  if (base::StartsWith(name, "proxy-", base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(name, "sec-", base::CompareCase::INSENSITIVE_ASCII))
    return false;

  for (const char* field : kForbiddenHeaderFields) {
    if (base::EqualsCaseInsensitiveASCII(name, field))
      return false;
  }

  bool is_forbidden_header_fields_with_forbidden_method = false;
  for (const char* field : kForbiddenHeaderFieldsWithForbiddenMethod) {
    if (base::EqualsCaseInsensitiveASCII(name, field)) {
      is_forbidden_header_fields_with_forbidden_method = true;
      break;
    }
  }
  if (is_forbidden_header_fields_with_forbidden_method) {
    ValuesIterator method_iterator(value, ',');
    while (method_iterator.GetNext()) {
      std::string_view method = method_iterator.value();
      for (const char* forbidden_method : kForbiddenMethods) {
        if (base::EqualsCaseInsensitiveASCII(method, forbidden_method))
          return false;
      }
    }
  }
  return true;
}

// static
bool HttpUtil::IsValidHeaderName(std::string_view name) {
  // Check whether the header name is RFC 2616-compliant.
  return HttpUtil::IsToken(name);
}

// static
bool HttpUtil::IsValidHeaderValue(std::string_view value) {
  // Just a sanity check: disallow NUL, CR and LF.
  for (char c : value) {
    if (c == '\0' || c == '\r' || c == '\n')
      return false;
  }
  return true;
}

// static
bool HttpUtil::IsNonCoalescingHeader(std::string_view name) {
  // NOTE: "set-cookie2" headers do not support expires attributes, so we don't
  // have to list them here.
  // As of 2023, using FlatSet here actually makes the lookup slower, and
  // unordered_set is even slower than that.
  static constexpr std::string_view kNonCoalescingHeaders[] = {
      "date", "expires", "last-modified",
      "location",  // See bug 1050541 for details
      "retry-after", "set-cookie",
      // The format of auth-challenges mixes both space separated tokens and
      // comma separated properties, so coalescing on comma won't work.
      "www-authenticate", "proxy-authenticate",
      // STS specifies that UAs must not process any STS headers after the first
      // one.
      "strict-transport-security"};

  for (std::string_view header : kNonCoalescingHeaders) {
    if (base::EqualsCaseInsensitiveASCII(name, header)) {
      return true;
    }
  }
  return false;
}

// static
void HttpUtil::TrimLWS(std::string::const_iterator* begin,
                       std::string::const_iterator* end) {
  TrimLWSImplementation(begin, end);
}

// static
std::string_view HttpUtil::TrimLWS(std::string_view string) {
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  TrimLWSImplementation(&begin, &end);
  return std::string_view(begin, end - begin);
}

bool HttpUtil::IsTokenChar(char c) {
  return !(c >= 0x7F || c <= 0x20 || c == '(' || c == ')' || c == '<' ||
           c == '>' || c == '@' || c == ',' || c == ';' || c == ':' ||
           c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' ||
           c == '?' || c == '=' || c == '{' || c == '}');
}

// See RFC 7230 Sec 3.2.6 for the definition of |token|.
bool HttpUtil::IsToken(std::string_view string) {
  if (string.empty())
    return false;
  for (char c : string) {
    if (!IsTokenChar(c))
      return false;
  }
  return true;
}

// See RFC 5987 Sec 3.2.1 for the definition of |parmname|.
bool HttpUtil::IsParmName(std::string_view str) {
  if (str.empty())
    return false;
  for (char c : str) {
    if (!IsTokenChar(c) || c == '*' || c == '\'' || c == '%')
      return false;
  }
  return true;
}

namespace {

bool IsQuote(char c) {
  return c == '"';
}

bool UnquoteImpl(std::string_view str, bool strict_quotes, std::string* out) {
  if (str.empty())
    return false;

  // Nothing to unquote.
  if (!IsQuote(str[0]))
    return false;

  // No terminal quote mark.
  if (str.size() < 2 || str.front() != str.back())
    return false;

  // Strip quotemarks
  str.remove_prefix(1);
  str.remove_suffix(1);

  // Unescape quoted-pair (defined in RFC 2616 section 2.2)
  bool prev_escape = false;
  std::string unescaped;
  for (char c : str) {
    if (c == '\\' && !prev_escape) {
      prev_escape = true;
      continue;
    }
    if (strict_quotes && !prev_escape && IsQuote(c))
      return false;
    prev_escape = false;
    unescaped.push_back(c);
  }

  // Terminal quote is escaped.
  if (strict_quotes && prev_escape)
    return false;

  *out = std::move(unescaped);
  return true;
}

}  // anonymous namespace

// static
std::string HttpUtil::Unquote(std::string_view str) {
  std::string result;
  if (!UnquoteImpl(str, false, &result))
    return std::string(str);

  return result;
}

// static
bool HttpUtil::StrictUnquote(std::string_view str, std::string* out) {
  return UnquoteImpl(str, true, out);
}

// static
std::string HttpUtil::Quote(std::string_view str) {
  std::string escaped;
  escaped.reserve(2 + str.size());

  // Esape any backslashes or quotemarks within the string, and
  // then surround with quotes.
  escaped.push_back('"');
  for (char c : str) {
    if (c == '"' || c == '\\')
      escaped.push_back('\\');
    escaped.push_back(c);
  }
  escaped.push_back('"');
  return escaped;
}

// Find the "http" substring in a status line. This allows for
// some slop at the start. If the "http" string could not be found
// then returns std::string::npos.
// static
size_t HttpUtil::LocateStartOfStatusLine(base::span<const uint8_t> buf) {
  const size_t slop = 4;
  const size_t http_len = 4;

  if (buf.size() >= http_len) {
    size_t i_max = std::min(buf.size() - http_len, slop);
    for (size_t i = 0; i <= i_max; ++i) {
      if (base::EqualsCaseInsensitiveASCII(
              base::as_string_view(buf.subspan(i, http_len)), "http")) {
        return i;
      }
    }
  }
  return std::string::npos;  // Not found
}

static size_t LocateEndOfHeadersHelper(base::span<const uint8_t> buf,
                                       size_t i,
                                       bool accept_empty_header_list) {
  char last_c = '\0';
  bool was_lf = false;
  if (accept_empty_header_list) {
    // Normally two line breaks signal the end of a header list. An empty header
    // list ends with a single line break at the start of the buffer.
    last_c = '\n';
    was_lf = true;
  }

  for (; i < buf.size(); ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (was_lf)
        return i + 1;
      was_lf = true;
    } else if (c != '\r' || last_c != '\n') {
      was_lf = false;
    }
    last_c = c;
  }
  return std::string::npos;
}

size_t HttpUtil::LocateEndOfAdditionalHeaders(base::span<const uint8_t> buf,
                                              size_t i) {
  return LocateEndOfHeadersHelper(buf, i, true);
}

size_t HttpUtil::LocateEndOfHeaders(base::span<const uint8_t> buf, size_t i) {
  return LocateEndOfHeadersHelper(buf, i, false);
}

// In order for a line to be continuable, it must specify a
// non-blank header-name. Line continuations are specifically for
// header values -- do not allow headers names to span lines.
static bool IsLineSegmentContinuable(std::string_view line) {
  if (line.empty())
    return false;

  size_t colon = line.find(':');
  if (colon == std::string_view::npos) {
    return false;
  }

  std::string_view name = line.substr(0, colon);

  // Name can't be empty.
  if (name.empty())
    return false;

  // Can't start with LWS (this would imply the segment is a continuation)
  if (HttpUtil::IsLWS(name[0]))
    return false;

  return true;
}

// Helper used by AssembleRawHeaders, to find the end of the status line.
static size_t FindStatusLineEnd(std::string_view str) {
  size_t i = str.find_first_of("\r\n");
  if (i == std::string_view::npos) {
    return str.size();
  }
  return i;
}

// Helper used by AssembleRawHeaders, to skip past leading LWS.
static std::string_view RemoveLeadingNonLWS(std::string_view str) {
  for (size_t i = 0; i < str.size(); i++) {
    if (!HttpUtil::IsLWS(str[i]))
      return str.substr(i);
  }
  return std::string_view();  // Remove everything.
}

std::string HttpUtil::AssembleRawHeaders(std::string_view input) {
  std::string raw_headers;
  raw_headers.reserve(input.size());

  // Skip any leading slop, since the consumers of this output
  // (HttpResponseHeaders) don't deal with it.
  size_t status_begin_offset =
      LocateStartOfStatusLine(base::as_byte_span(input));
  if (status_begin_offset != std::string::npos)
    input.remove_prefix(status_begin_offset);

  // Copy the status line.
  size_t status_line_end = FindStatusLineEnd(input);
  raw_headers.append(input.data(), status_line_end);
  input.remove_prefix(status_line_end);

  // After the status line, every subsequent line is a header line segment.
  // Should a segment start with LWS, it is a continuation of the previous
  // line's field-value.

  // TODO(ericroman): is this too permissive? (delimits on [\r\n]+)
  base::CStringTokenizer lines(input.data(), input.data() + input.size(),
                               "\r\n");

  // This variable is true when the previous line was continuable.
  bool prev_line_continuable = false;

  while (lines.GetNext()) {
    std::string_view line = lines.token_piece();

    if (prev_line_continuable && IsLWS(line[0])) {
      // Join continuation; reduce the leading LWS to a single SP.
      base::StrAppend(&raw_headers, {" ", RemoveLeadingNonLWS(line)});
    } else {
      // Terminate the previous line and copy the raw data to output.
      base::StrAppend(&raw_headers, {"\n", line});

      // Check if the current line can be continued.
      prev_line_continuable = IsLineSegmentContinuable(line);
    }
  }

  raw_headers.append("\n\n", 2);

  // Use '\0' as the canonical line terminator. If the input already contained
  // any embeded '\0' characters we will strip them first to avoid interpreting
  // them as line breaks.
  std::erase(raw_headers, '\0');

  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');

  return raw_headers;
}

std::string HttpUtil::ConvertHeadersBackToHTTPResponse(const std::string& str) {
  std::string disassembled_headers;
  base::StringTokenizer tokenizer(str, std::string(1, '\0'));
  while (tokenizer.GetNext()) {
    base::StrAppend(&disassembled_headers, {tokenizer.token_piece(), "\r\n"});
  }
  disassembled_headers.append("\r\n");

  return disassembled_headers;
}

std::string HttpUtil::ExpandLanguageList(const std::string& language_prefs) {
  const std::vector<std::string> languages = base::SplitString(
      language_prefs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (languages.empty())
    return "";

  AcceptLanguageBuilder builder;

  const size_t size = languages.size();
  for (size_t i = 0; i < size; ++i) {
    const std::string& language = languages[i];
    builder.AddLanguageCode(language);

    // Extract the primary language subtag.
    const std::string& base_language = GetBaseLanguageCode(language);

    // Skip 'x' and 'i' as a primary language subtag per RFC 5646 section 2.1.1.
    if (base_language == "x" || base_language == "i")
      continue;

    // Look ahead and add the primary language subtag as a language if the next
    // language is not part of the same family. This may not be perfect because
    // an input of "en-US,fr,en" will yield "en-US,en,fr,en" and later make "en"
    // a higher priority than "fr" despite the original preference.
    const size_t j = i + 1;
    if (j >= size || GetBaseLanguageCode(languages[j]) != base_language) {
      builder.AddLanguageCode(base_language);
    }
  }

  return builder.GetString();
}

// TODO(jungshik): This function assumes that the input is a comma separated
// list without any whitespace. As long as it comes from the preference and
// a user does not manually edit the preference file, it's the case. Still,
// we may have to make it more robust.
std::string HttpUtil::GenerateAcceptLanguageHeader(
    const std::string& raw_language_list) {
  // We use integers for qvalue and qvalue decrement that are 10 times
  // larger than actual values to avoid a problem with comparing
  // two floating point numbers.
  const unsigned int kQvalueDecrement10 = 1;
  unsigned int qvalue10 = 10;
  base::StringTokenizer t(raw_language_list, ",");
  std::string lang_list_with_q;
  while (t.GetNext()) {
    std::string language = t.token();
    if (qvalue10 == 10) {
      // q=1.0 is implicit.
      lang_list_with_q = language;
    } else {
      DCHECK_LT(qvalue10, 10U);
      base::StringAppendF(&lang_list_with_q, ",%s;q=0.%d", language.c_str(),
                          qvalue10);
    }
    // It does not make sense to have 'q=0'.
    if (qvalue10 > kQvalueDecrement10)
      qvalue10 -= kQvalueDecrement10;
  }
  return lang_list_with_q;
}

bool HttpUtil::HasStrongValidators(
    HttpVersion version,
    std::optional<std::string_view> etag_header,
    std::optional<std::string_view> last_modified_header,
    std::optional<std::string_view> date_header) {
  if (version < HttpVersion(1, 1))
    return false;

  if (etag_header && !etag_header->empty()) {
    size_t slash = etag_header->find('/');
    if (slash == std::string_view::npos || slash == 0) {
      return true;
    }

    std::string_view trimmed_etag = TrimLWS(etag_header->substr(0, slash));
    if (!base::EqualsCaseInsensitiveASCII(trimmed_etag, "w")) {
      return true;
    }
  }

  base::Time last_modified;
  if (!last_modified_header ||
      !base::Time::FromString(std::string(*last_modified_header).c_str(),
                              &last_modified)) {
    return false;
  }

  base::Time date;
  if (!date_header ||
      !base::Time::FromString(std::string(*date_header).c_str(), &date)) {
    return false;
  }

  // Last-Modified is implicitly weak unless it is at least 60 seconds before
  // the Date value.
  return ((date - last_modified).InSeconds() >= 60);
}

bool HttpUtil::HasValidators(
    HttpVersion version,
    std::optional<std::string_view> etag_header,
    std::optional<std::string_view> last_modified_header) {
  if (version < HttpVersion(1, 0))
    return false;

  base::Time last_modified;
  // Have to construct a C-style string here, since that's what
  // base::Time::FromString requires.
  if (last_modified_header &&
      base::Time::FromString(std::string(*last_modified_header).c_str(),
                             &last_modified)) {
    return true;
  }

  // It is OK to consider an empty string in etag_header to be a missing header
  // since valid ETags are always quoted-strings (see RFC 2616 3.11) and thus
  // empty ETags aren't empty strings (i.e., an empty ETag might be "\"\"").
  return version >= HttpVersion(1, 1) && etag_header && !etag_header->empty();
}

// Functions for histogram initialization.  The code 0 is put in the map to
// track status codes that are invalid.
// TODO(gavinp): Greatly prune the collected codes once we learn which
// ones are not sent in practice, to reduce upload size & memory use.

enum {
  HISTOGRAM_MIN_HTTP_STATUS_CODE = 100,
  HISTOGRAM_MAX_HTTP_STATUS_CODE = 599,
};

// static
std::vector<int> HttpUtil::GetStatusCodesForHistogram() {
  std::vector<int> codes;
  codes.reserve(
      HISTOGRAM_MAX_HTTP_STATUS_CODE - HISTOGRAM_MIN_HTTP_STATUS_CODE + 2);
  codes.push_back(0);
  for (int i = HISTOGRAM_MIN_HTTP_STATUS_CODE;
       i <= HISTOGRAM_MAX_HTTP_STATUS_CODE; ++i)
    codes.push_back(i);
  return codes;
}

// static
int HttpUtil::MapStatusCodeForHistogram(int code) {
  if (HISTOGRAM_MIN_HTTP_STATUS_CODE <= code &&
      code <= HISTOGRAM_MAX_HTTP_STATUS_CODE)
    return code;
  return 0;
}

// BNF from section 4.2 of RFC 2616:
//
//   message-header = field-name ":" [ field-value ]
//   field-name     = token
//   field-value    = *( field-content | LWS )
//   field-content  = <the OCTETs making up the field-value
//                     and consisting of either *TEXT or combinations
//                     of token, separators, and quoted-string>
//

HttpUtil::HeadersIterator::HeadersIterator(
    std::string::const_iterator headers_begin,
    std::string::const_iterator headers_end,
    const std::string& line_delimiter)
    : lines_(headers_begin, headers_end, line_delimiter) {
}

HttpUtil::HeadersIterator::~HeadersIterator() = default;

bool HttpUtil::HeadersIterator::GetNext() {
  while (lines_.GetNext()) {
    name_begin_ = lines_.token_begin();
    values_end_ = lines_.token_end();

    std::string::const_iterator colon(std::find(name_begin_, values_end_, ':'));
    if (colon == values_end_)
      continue;  // skip malformed header

    name_end_ = colon;

    // If the name starts with LWS, it is an invalid line.
    // Leading LWS implies a line continuation, and these should have
    // already been joined by AssembleRawHeaders().
    if (name_begin_ == name_end_ || IsLWS(*name_begin_))
      continue;

    TrimLWS(&name_begin_, &name_end_);
    DCHECK(name_begin_ < name_end_);
    if (!IsToken(base::MakeStringPiece(name_begin_, name_end_)))
      continue;  // skip malformed header

    values_begin_ = colon + 1;
    TrimLWS(&values_begin_, &values_end_);

    // if we got a header name, then we are done.
    return true;
  }
  return false;
}

bool HttpUtil::HeadersIterator::AdvanceTo(const char* name) {
  DCHECK(name != nullptr);
  DCHECK_EQ(0, base::ToLowerASCII(name).compare(name))
      << "the header name must be in all lower case";

  while (GetNext()) {
    if (base::EqualsCaseInsensitiveASCII(
            base::MakeStringPiece(name_begin_, name_end_), name)) {
      return true;
    }
  }

  return false;
}

HttpUtil::ValuesIterator::ValuesIterator(std::string_view values,
                                         char delimiter,
                                         bool ignore_empty_values)
    : values_(values, std::string(1, delimiter)),
      ignore_empty_values_(ignore_empty_values) {
  values_.set_quote_chars("\"");
  // Could set this unconditionally, since code below has to check for empty
  // values after trimming, anyways, but may provide a minor performance
  // improvement.
  if (!ignore_empty_values_)
    values_.set_options(base::StringTokenizer::RETURN_EMPTY_TOKENS);
}

HttpUtil::ValuesIterator::ValuesIterator(const ValuesIterator& other) = default;

HttpUtil::ValuesIterator::~ValuesIterator() = default;

bool HttpUtil::ValuesIterator::GetNext() {
  while (values_.GetNext()) {
    value_ = TrimLWS(values_.token());

    if (!ignore_empty_values_ || !value_.empty()) {
      return true;
    }
  }
  return false;
}

HttpUtil::NameValuePairsIterator::NameValuePairsIterator(std::string_view value,
                                                         char delimiter,
                                                         Values optional_values,
                                                         Quotes strict_quotes)
    : props_(value, delimiter),
      values_optional_(optional_values == Values::NOT_REQUIRED),
      strict_quotes_(strict_quotes == Quotes::STRICT_QUOTES) {}

HttpUtil::NameValuePairsIterator::NameValuePairsIterator(
    const NameValuePairsIterator& other) = default;

HttpUtil::NameValuePairsIterator::~NameValuePairsIterator() = default;

// We expect properties to be formatted as one of:
//   name="value"
//   name='value'
//   name='\'value\''
//   name=value
//   name = value
//   name (if values_optional_ is true)
// Due to buggy implementations found in some embedded devices, we also
// accept values with missing close quotemark (http://crbug.com/39836):
//   name="value
bool HttpUtil::NameValuePairsIterator::GetNext() {
  CHECK(valid_);
  // Not an error, but nothing left to do.
  if (props_.GetNext()) {
    // State only becomes invalid if there's another element, but parsing it
    // fails.
    valid_ = ParseNameValuePair(props_.value());
    if (valid_) {
      return true;
    }
  }

  // Clear all fields when returning false, regardless of whether `valid` is
  // true or not, since any populated data is no longer valid.
  name_ = std::string_view();
  value_ = std::string_view();
  unquoted_value_.clear();
  value_is_quoted_ = false;
  return false;
}

bool HttpUtil::NameValuePairsIterator::ParseNameValuePair(
    std::string_view name_value_pair) {
  // Scan for the equals sign.
  const size_t equals = name_value_pair.find('=');
  if (equals == 0) {
    return false;  // Malformed, no name
  }
  const bool has_value = (equals != std::string_view::npos);
  if (!has_value && !values_optional_) {
    return false;  // Malformed, no equals sign and values are required
  }

  // Make `name_` everything up until the equals sign.
  name_ = TrimLWS(name_value_pair.substr(0, equals));
  // Clear rest of state.
  value_ = std::string_view();
  value_is_quoted_ = false;
  unquoted_value_.clear();

  // If there is a value, do additional checking and calculate the value.
  if (has_value) {
    // Check that no quote appears before the equals sign.
    if (base::ranges::any_of(name_, IsQuote)) {
      return false;
    }

    // Value consists of everything after the equals sign, with whitespace
    // trimmed.
    value_ = TrimLWS(name_value_pair.substr(equals + 1));
    if (value_.empty()) {
      // Malformed; value is empty
      return false;
    }
  }

  if (has_value && IsQuote(value_.front())) {
    value_is_quoted_ = true;

    if (strict_quotes_) {
      return HttpUtil::StrictUnquote(value_, &unquoted_value_);
    }

    // Trim surrounding quotemarks off the value
    if (value_.front() != value_.back() || value_.size() == 1) {
      // NOTE: This is not as graceful as it sounds:
      // * quoted-pairs will no longer be unquoted
      //   (["\"hello] should give ["hello]).
      // * Does not detect when the final quote is escaped
      //   (["value\"] should give [value"])
      value_is_quoted_ = false;
      value_ = value_.substr(1);  // Gracefully recover from mismatching quotes.
    } else {
      // Do not store iterators into this. See declaration of `unquoted_value_`.
      unquoted_value_ = HttpUtil::Unquote(value_);
    }
  }

  return true;
}

bool HttpUtil::ParseAcceptEncoding(const std::string& accept_encoding,
                                   std::set<std::string>* allowed_encodings) {
  DCHECK(allowed_encodings);
  if (accept_encoding.find_first_of("\"") != std::string::npos)
    return false;
  allowed_encodings->clear();

  base::StringTokenizer tokenizer(accept_encoding.begin(),
                                  accept_encoding.end(), ",");
  while (tokenizer.GetNext()) {
    std::string_view entry = tokenizer.token_piece();
    entry = TrimLWS(entry);
    size_t semicolon_pos = entry.find(';');
    if (semicolon_pos == std::string_view::npos) {
      if (entry.find_first_of(HTTP_LWS) != std::string_view::npos) {
        return false;
      }
      allowed_encodings->insert(base::ToLowerASCII(entry));
      continue;
    }
    std::string_view encoding = entry.substr(0, semicolon_pos);
    encoding = TrimLWS(encoding);
    if (encoding.find_first_of(HTTP_LWS) != std::string_view::npos) {
      return false;
    }
    std::string_view params = entry.substr(semicolon_pos + 1);
    params = TrimLWS(params);
    size_t equals_pos = params.find('=');
    if (equals_pos == std::string_view::npos) {
      return false;
    }
    std::string_view param_name = params.substr(0, equals_pos);
    param_name = TrimLWS(param_name);
    if (!base::EqualsCaseInsensitiveASCII(param_name, "q"))
      return false;
    std::string_view qvalue = params.substr(equals_pos + 1);
    qvalue = TrimLWS(qvalue);
    if (qvalue.empty())
      return false;
    if (qvalue[0] == '1') {
      if (std::string_view("1.000").starts_with(qvalue)) {
        allowed_encodings->insert(base::ToLowerASCII(encoding));
        continue;
      }
      return false;
    }
    if (qvalue[0] != '0')
      return false;
    if (qvalue.length() == 1)
      continue;
    if (qvalue.length() <= 2 || qvalue.length() > 5)
      return false;
    if (qvalue[1] != '.')
      return false;
    bool nonzero_number = false;
    for (size_t i = 2; i < qvalue.length(); ++i) {
      if (!base::IsAsciiDigit(qvalue[i]))
        return false;
      if (qvalue[i] != '0')
        nonzero_number = true;
    }
    if (nonzero_number)
      allowed_encodings->insert(base::ToLowerASCII(encoding));
  }

  // RFC 7231 5.3.4 "A request without an Accept-Encoding header field implies
  // that the user agent has no preferences regarding content-codings."
  if (allowed_encodings->empty()) {
    allowed_encodings->insert("*");
    return true;
  }

  // Any browser must support "identity".
  allowed_encodings->insert("identity");

  // RFC says gzip == x-gzip; mirror it here for easier matching.
  if (allowed_encodings->find("gzip") != allowed_encodings->end())
    allowed_encodings->insert("x-gzip");
  if (allowed_encodings->find("x-gzip") != allowed_encodings->end())
    allowed_encodings->insert("gzip");

  // RFC says compress == x-compress; mirror it here for easier matching.
  if (allowed_encodings->find("compress") != allowed_encodings->end())
    allowed_encodings->insert("x-compress");
  if (allowed_encodings->find("x-compress") != allowed_encodings->end())
    allowed_encodings->insert("compress");
  return true;
}

bool HttpUtil::ParseContentEncoding(const std::string& content_encoding,
                                    std::set<std::string>* used_encodings) {
  DCHECK(used_encodings);
  if (content_encoding.find_first_of("\"=;*") != std::string::npos)
    return false;
  used_encodings->clear();

  base::StringTokenizer encoding_tokenizer(content_encoding.begin(),
                                           content_encoding.end(), ",");
  while (encoding_tokenizer.GetNext()) {
    std::string_view encoding = TrimLWS(encoding_tokenizer.token_piece());
    if (encoding.find_first_of(HTTP_LWS) != std::string_view::npos) {
      return false;
    }
    used_encodings->insert(base::ToLowerASCII(encoding));
  }
  return true;
}

bool HttpUtil::HeadersContainMultipleCopiesOfField(
    const HttpResponseHeaders& headers,
    const std::string& field_name) {
  size_t it = 0;
  std::optional<std::string_view> field_value =
      headers.EnumerateHeader(&it, field_name);
  if (!field_value) {
    return false;
  }
  // There's at least one `field_name` header.  Check if there are any more
  // such headers, and if so, return true if they have different values.
  std::optional<std::string_view> field_value2;
  while ((field_value2 = headers.EnumerateHeader(&it, field_name))) {
    if (field_value != field_value2)
      return true;
  }
  return false;
}

}  // namespace net
