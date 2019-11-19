// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/url_util.h"

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

// static
std::string HttpUtil::SpecForRequest(const GURL& url) {
  DCHECK(url.is_valid() &&
         (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS()));
  return SimplifyUrlForRequest(url).spec();
}

// static
void HttpUtil::ParseContentType(const std::string& content_type_str,
                                std::string* mime_type,
                                std::string* charset,
                                bool* had_charset,
                                std::string* boundary) {
  const std::string::const_iterator begin = content_type_str.begin();

  // Trim leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(HTTP_LWS);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end = content_type_str.find_first_of(HTTP_LWS ";(", type_val);
  if (type_end == std::string::npos)
    type_end = content_type_str.length();

  std::string charset_value;
  bool type_has_charset = false;
  bool type_has_boundary = false;

  // Iterate over parameters. Can't split the string around semicolons
  // preemptively because quoted strings may include semicolons. Mostly matches
  // logic in https://mimesniff.spec.whatwg.org/. Main differences: Does not
  // validate characters are HTTP token code points / HTTP quoted-string token
  // code points, and ignores spaces after "=" in parameters.
  std::string::size_type offset = content_type_str.find_first_of(';', type_end);
  while (offset < content_type_str.size()) {
    DCHECK_EQ(';', content_type_str[offset]);
    // Trim off the semicolon.
    ++offset;

    // Trim off any following spaces.
    offset = content_type_str.find_first_not_of(HTTP_LWS, offset);
    std::string::size_type param_name_start = offset;

    // Extend parameter name until run into a semicolon or equals sign.  Per
    // spec, trailing spaces are not removed.
    offset = content_type_str.find_first_of(";=", offset);

    // Nothing more to do if at end of string, or if there's no parameter
    // value, since names without values aren't allowed.
    if (offset == std::string::npos || content_type_str[offset] == ';')
      continue;

    base::StringPiece param_name(content_type_str.begin() + param_name_start,
                                 content_type_str.begin() + offset);

    // Now parse the value.
    DCHECK_EQ('=', content_type_str[offset]);
    // Trim off the '='.
    offset++;

    // Remove leading spaces. This violates the spec, though it matches
    // pre-existing behavior.
    //
    // TODO(mmenke): Consider doing this (only?) after parsing quotes, which
    // seems to align more with the spec - not the content-type spec, but the
    // GET spec's way of getting an encoding, and the spec for handling
    // boundary values as well.
    // See https://encoding.spec.whatwg.org/#names-and-labels.
    offset = content_type_str.find_first_not_of(HTTP_LWS, offset);

    std::string param_value;
    if (offset == std::string::npos || content_type_str[offset] == ';') {
      // Nothing to do here - an unquoted string of only whitespace should be
      // skipped.
      continue;
    } else if (content_type_str[offset] != '"') {
      // If the first character is not a quotation mark, copy data directly.
      std::string::size_type value_start = offset;
      offset = content_type_str.find_first_of(';', offset);
      std::string::size_type value_end = offset;

      // Remove terminal whitespace. If ran off the end of the string, have to
      // update |value_end| first.
      if (value_end == std::string::npos)
        value_end = content_type_str.size();
      while (value_end > value_start &&
             IsLWS(content_type_str[value_end - 1])) {
        --value_end;
      }

      param_value =
          content_type_str.substr(value_start, value_end - value_start);
    } else {
      // Otherwise, append data, with special handling for backslashes, until
      // a close quote.

      // Skip open quote.
      DCHECK_EQ('"', content_type_str[offset]);
      ++offset;

      while (offset < content_type_str.size() &&
             content_type_str[offset] != '"') {
        // Skip over backslash and append the next character, when not at
        // the end of the string. Otherwise, copy the next character (Which may
        // be a backslash).
        if (content_type_str[offset] == '\\' &&
            offset + 1 < content_type_str.size()) {
          ++offset;
        }
        param_value += content_type_str[offset];
        ++offset;
      }

      param_value = TrimLWS(param_value).as_string();

      offset = content_type_str.find_first_of(';', offset);
    }

    // TODO(mmenke): Check that name has only valid characters.
    if (!type_has_charset &&
        base::LowerCaseEqualsASCII(param_name, "charset")) {
      type_has_charset = true;
      charset_value = param_value;
      continue;
    }

    if (boundary && !type_has_boundary &&
        base::LowerCaseEqualsASCII(param_name, "boundary")) {
      type_has_boundary = true;
      boundary->assign(std::move(param_value));
      continue;
    }
  }

  // If the server sent "*/*", it is meaningless, so do not store it.
  // Also, reject a mime-type if it does not include a slash.
  // Some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() == 0 || content_type_str == "*/*" ||
      content_type_str.find_first_of('/') == std::string::npos) {
    return;
  }

  // If type_val is the same as mime_type, then just update the charset.
  // However, if charset is empty and mime_type hasn't changed, then don't
  // wipe-out an existing charset.
  // It is common that mime_type is empty.
  bool eq = !mime_type->empty() &&
            base::LowerCaseEqualsASCII(
                base::StringPiece(begin + type_val, begin + type_end),
                mime_type->data());
  if (!eq) {
    *mime_type = base::ToLowerASCII(
        base::StringPiece(begin + type_val, begin + type_end));
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
  base::StringPiece bytes_unit =
      base::StringPiece(ranges_specifier).substr(0, equal_char_offset);

  // "bytes" unit identifier is not found.
  bytes_unit = TrimLWS(bytes_unit);
  if (!base::LowerCaseEqualsASCII(bytes_unit, "bytes")) {
    return false;
  }

  std::string::const_iterator byte_range_set_begin =
      ranges_specifier.begin() + equal_char_offset + 1;
  std::string::const_iterator byte_range_set_end = ranges_specifier.end();

  ValuesIterator byte_range_set_iterator(byte_range_set_begin,
                                         byte_range_set_end, ',');
  while (byte_range_set_iterator.GetNext()) {
    base::StringPiece value = byte_range_set_iterator.value_piece();
    size_t minus_char_offset = value.find('-');
    // If '-' character is not found, reports failure.
    if (minus_char_offset == std::string::npos)
      return false;

    base::StringPiece first_byte_pos = value.substr(0, minus_char_offset);
    first_byte_pos = TrimLWS(first_byte_pos);

    HttpByteRange range;
    // Try to obtain first-byte-pos.
    if (!first_byte_pos.empty()) {
      int64_t first_byte_position = -1;
      if (!base::StringToInt64(first_byte_pos, &first_byte_position))
        return false;
      range.set_first_byte_position(first_byte_position);
    }

    base::StringPiece last_byte_pos = value.substr(minus_char_offset + 1);
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
    base::StringPiece content_range_spec,
    int64_t* first_byte_position,
    int64_t* last_byte_position,
    int64_t* instance_length) {
  *first_byte_position = *last_byte_position = *instance_length = -1;
  content_range_spec = TrimLWS(content_range_spec);

  size_t space_position = content_range_spec.find(' ');
  if (space_position == base::StringPiece::npos)
    return false;

  // Invalid header if it doesn't contain "bytes-unit".
  if (!base::LowerCaseEqualsASCII(
          TrimLWS(content_range_spec.substr(0, space_position)), "bytes")) {
    return false;
  }

  size_t minus_position = content_range_spec.find('-', space_position + 1);
  if (minus_position == base::StringPiece::npos)
    return false;
  size_t slash_position = content_range_spec.find('/', minus_position + 1);
  if (slash_position == base::StringPiece::npos)
    return false;

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
  int seconds;
  base::Time time;
  base::TimeDelta interval;

  if (base::StringToInt(retry_after_string, &seconds)) {
    interval = base::TimeDelta::FromSeconds(seconds);
  } else if (base::Time::FromUTCString(retry_after_string.c_str(), &time)) {
    interval = time - now;
  } else {
    return false;
  }

  if (interval < base::TimeDelta::FromSeconds(0))
    return false;

  *retry_after = interval;
  return true;
}

namespace {

// A header string containing any of the following fields will cause
// an error. The list comes from the fetch standard.
const char* const kForbiddenHeaderFields[] = {
    "accept-charset",
    "accept-encoding",
    "access-control-request-headers",
    "access-control-request-method",
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
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    // TODO(mmenke): This is no longer banned, but still here due to issues
    // mentioned in https://crbug.com/571722.
    "user-agent",
    "via",
};

}  // namespace

// static
bool HttpUtil::IsMethodSafe(base::StringPiece method) {
  return method == "GET" || method == "HEAD" || method == "OPTIONS" ||
         method == "TRACE";
}

// static
bool HttpUtil::IsMethodIdempotent(base::StringPiece method) {
  return IsMethodSafe(method) || method == "PUT" || method == "DELETE";
}

// static
bool HttpUtil::IsSafeHeader(base::StringPiece name) {
  if (base::StartsWith(name, "proxy-", base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(name, "sec-", base::CompareCase::INSENSITIVE_ASCII))
    return false;

  for (const char* field : kForbiddenHeaderFields) {
    if (base::LowerCaseEqualsASCII(name, field))
      return false;
  }
  return true;
}

// static
bool HttpUtil::IsValidHeaderName(base::StringPiece name) {
  // Check whether the header name is RFC 2616-compliant.
  return HttpUtil::IsToken(name);
}

// static
bool HttpUtil::IsValidHeaderValue(base::StringPiece value) {
  // Just a sanity check: disallow NUL, CR and LF.
  for (char c : value) {
    if (c == '\0' || c == '\r' || c == '\n')
      return false;
  }
  return true;
}

// static
bool HttpUtil::IsNonCoalescingHeader(base::StringPiece name) {
  // NOTE: "set-cookie2" headers do not support expires attributes, so we don't
  // have to list them here.
  const char* const kNonCoalescingHeaders[] = {
    "date",
    "expires",
    "last-modified",
    "location",  // See bug 1050541 for details
    "retry-after",
    "set-cookie",
    // The format of auth-challenges mixes both space separated tokens and
    // comma separated properties, so coalescing on comma won't work.
    "www-authenticate",
    "proxy-authenticate",
    // STS specifies that UAs must not process any STS headers after the first
    // one.
    "strict-transport-security"
  };

  for (const char* header : kNonCoalescingHeaders) {
    if (base::LowerCaseEqualsASCII(name, header)) {
      return true;
    }
  }
  return false;
}

bool HttpUtil::IsLWS(char c) {
  const base::StringPiece kWhiteSpaceCharacters(HTTP_LWS);
  return kWhiteSpaceCharacters.find(c) != base::StringPiece::npos;
}

// static
void HttpUtil::TrimLWS(std::string::const_iterator* begin,
                       std::string::const_iterator* end) {
  TrimLWSImplementation(begin, end);
}

// static
base::StringPiece HttpUtil::TrimLWS(const base::StringPiece& string) {
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  TrimLWSImplementation(&begin, &end);
  return base::StringPiece(begin, end - begin);
}

bool HttpUtil::IsTokenChar(char c) {
  return !(c >= 0x7F || c <= 0x20 || c == '(' || c == ')' || c == '<' ||
           c == '>' || c == '@' || c == ',' || c == ';' || c == ':' ||
           c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' ||
           c == '?' || c == '=' || c == '{' || c == '}');
}

// See RFC 7230 Sec 3.2.6 for the definition of |token|.
bool HttpUtil::IsToken(base::StringPiece string) {
  if (string.empty())
    return false;
  for (char c : string) {
    if (!IsTokenChar(c))
      return false;
  }
  return true;
}

// See RFC 5987 Sec 3.2.1 for the definition of |parmname|.
bool HttpUtil::IsParmName(base::StringPiece str) {
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

bool UnquoteImpl(base::StringPiece str, bool strict_quotes, std::string* out) {
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
std::string HttpUtil::Unquote(base::StringPiece str) {
  std::string result;
  if (!UnquoteImpl(str, false, &result))
    return str.as_string();

  return result;
}

// static
bool HttpUtil::StrictUnquote(base::StringPiece str, std::string* out) {
  return UnquoteImpl(str, true, out);
}

// static
std::string HttpUtil::Quote(base::StringPiece str) {
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
size_t HttpUtil::LocateStartOfStatusLine(const char* buf, size_t buf_len) {
  const size_t slop = 4;
  const size_t http_len = 4;

  if (buf_len >= http_len) {
    size_t i_max = std::min(buf_len - http_len, slop);
    for (size_t i = 0; i <= i_max; ++i) {
      if (base::LowerCaseEqualsASCII(base::StringPiece(buf + i, http_len),
                                     "http"))
        return i;
    }
  }
  return std::string::npos;  // Not found
}

static size_t LocateEndOfHeadersHelper(const char* buf,
                                       size_t buf_len,
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

  for (; i < buf_len; ++i) {
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

size_t HttpUtil::LocateEndOfAdditionalHeaders(const char* buf,
                                              size_t buf_len,
                                              size_t i) {
  return LocateEndOfHeadersHelper(buf, buf_len, i, true);
}

size_t HttpUtil::LocateEndOfHeaders(const char* buf, size_t buf_len, size_t i) {
  return LocateEndOfHeadersHelper(buf, buf_len, i, false);
}

// In order for a line to be continuable, it must specify a
// non-blank header-name. Line continuations are specifically for
// header values -- do not allow headers names to span lines.
static bool IsLineSegmentContinuable(base::StringPiece line) {
  if (line.empty())
    return false;

  size_t colon = line.find(':');
  if (colon == base::StringPiece::npos)
    return false;

  base::StringPiece name = line.substr(0, colon);

  // Name can't be empty.
  if (name.empty())
    return false;

  // Can't start with LWS (this would imply the segment is a continuation)
  if (HttpUtil::IsLWS(name[0]))
    return false;

  return true;
}

// Helper used by AssembleRawHeaders, to find the end of the status line.
static size_t FindStatusLineEnd(base::StringPiece str) {
  size_t i = str.find_first_of("\r\n");
  if (i == base::StringPiece::npos)
    return str.size();
  return i;
}

// Helper used by AssembleRawHeaders, to skip past leading LWS.
static base::StringPiece RemoveLeadingNonLWS(base::StringPiece str) {
  for (size_t i = 0; i < str.size(); i++) {
    if (!HttpUtil::IsLWS(str[i]))
      return str.substr(i);
  }
  return base::StringPiece();  // Remove everything.
}

std::string HttpUtil::AssembleRawHeaders(base::StringPiece input) {
  std::string raw_headers;
  raw_headers.reserve(input.size());

  // Skip any leading slop, since the consumers of this output
  // (HttpResponseHeaders) don't deal with it.
  size_t status_begin_offset =
      LocateStartOfStatusLine(input.data(), input.size());
  if (status_begin_offset != std::string::npos)
    input.remove_prefix(status_begin_offset);

  // Copy the status line.
  size_t status_line_end = FindStatusLineEnd(input);
  input.substr(0, status_line_end).AppendToString(&raw_headers);
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
    base::StringPiece line = lines.token_piece();

    if (prev_line_continuable && IsLWS(line[0])) {
      // Join continuation; reduce the leading LWS to a single SP.
      raw_headers.push_back(' ');
      RemoveLeadingNonLWS(line).AppendToString(&raw_headers);
    } else {
      // Terminate the previous line.
      raw_headers.push_back('\n');

      // Copy the raw data to output.
      line.AppendToString(&raw_headers);

      // Check if the current line can be continued.
      prev_line_continuable = IsLineSegmentContinuable(line);
    }
  }

  raw_headers.append("\n\n", 2);

  // Use '\0' as the canonical line terminator. If the input already contained
  // any embeded '\0' characters we will strip them first to avoid interpreting
  // them as line breaks.
  base::Erase(raw_headers, '\0');

  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');

  return raw_headers;
}

std::string HttpUtil::ConvertHeadersBackToHTTPResponse(const std::string& str) {
  std::string disassembled_headers;
  base::StringTokenizer tokenizer(str, std::string(1, '\0'));
  while (tokenizer.GetNext()) {
    tokenizer.token_piece().AppendToString(&disassembled_headers);
    disassembled_headers.append("\r\n");
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

    // Extract the base language
    const std::string& base_language = GetBaseLanguageCode(language);

    // Look ahead and add the base language if the next language is not part
    // of the same family.
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

bool HttpUtil::HasStrongValidators(HttpVersion version,
                                   const std::string& etag_header,
                                   const std::string& last_modified_header,
                                   const std::string& date_header) {
  if (!HasValidators(version, etag_header, last_modified_header))
    return false;

  if (version < HttpVersion(1, 1))
    return false;

  if (!etag_header.empty()) {
    size_t slash = etag_header.find('/');
    if (slash == std::string::npos || slash == 0)
      return true;

    std::string::const_iterator i = etag_header.begin();
    std::string::const_iterator j = etag_header.begin() + slash;
    TrimLWS(&i, &j);
    if (!base::LowerCaseEqualsASCII(base::StringPiece(i, j), "w"))
      return true;
  }

  base::Time last_modified;
  if (!base::Time::FromString(last_modified_header.c_str(), &last_modified))
    return false;

  base::Time date;
  if (!base::Time::FromString(date_header.c_str(), &date))
    return false;

  // Last-Modified is implicitly weak unless it is at least 60 seconds before
  // the Date value.
  return ((date - last_modified).InSeconds() >= 60);
}

bool HttpUtil::HasValidators(HttpVersion version,
                             const std::string& etag_header,
                             const std::string& last_modified_header) {
  if (version < HttpVersion(1, 0))
    return false;

  base::Time last_modified;
  if (base::Time::FromString(last_modified_header.c_str(), &last_modified))
    return true;

  // It is OK to consider an empty string in etag_header to be a missing header
  // since valid ETags are always quoted-strings (see RFC 2616 3.11) and thus
  // empty ETags aren't empty strings (i.e., an empty ETag might be "\"\"").
  return version >= HttpVersion(1, 1) && !etag_header.empty();
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
    if (!IsToken(base::StringPiece(name_begin_, name_end_)))
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
    if (base::LowerCaseEqualsASCII(base::StringPiece(name_begin_, name_end_),
                                   name)) {
      return true;
    }
  }

  return false;
}

HttpUtil::ValuesIterator::ValuesIterator(
    std::string::const_iterator values_begin,
    std::string::const_iterator values_end,
    char delimiter,
    bool ignore_empty_values)
    : values_(values_begin, values_end, std::string(1, delimiter)),
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
    value_begin_ = values_.token_begin();
    value_end_ = values_.token_end();
    TrimLWS(&value_begin_, &value_end_);

    if (!ignore_empty_values_ || value_begin_ != value_end_)
      return true;
  }
  return false;
}

HttpUtil::NameValuePairsIterator::NameValuePairsIterator(
    std::string::const_iterator begin,
    std::string::const_iterator end,
    char delimiter,
    Values optional_values,
    Quotes strict_quotes)
    : props_(begin, end, delimiter),
      valid_(true),
      name_begin_(end),
      name_end_(end),
      value_begin_(end),
      value_end_(end),
      value_is_quoted_(false),
      values_optional_(optional_values == Values::NOT_REQUIRED),
      strict_quotes_(strict_quotes == Quotes::STRICT_QUOTES) {}

HttpUtil::NameValuePairsIterator::NameValuePairsIterator(
    std::string::const_iterator begin,
    std::string::const_iterator end,
    char delimiter)
    : NameValuePairsIterator(begin,
                             end,
                             delimiter,
                             Values::REQUIRED,
                             Quotes::NOT_STRICT) {}

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
  if (!props_.GetNext())
    return false;

  // Set the value as everything. Next we will split out the name.
  value_begin_ = props_.value_begin();
  value_end_ = props_.value_end();
  name_begin_ = name_end_ = value_end_;

  // Scan for the equals sign.
  std::string::const_iterator equals = std::find(value_begin_, value_end_, '=');
  if (equals == value_begin_)
    return valid_ = false;  // Malformed, no name
  if (equals == value_end_ && !values_optional_)
    return valid_ = false;  // Malformed, no equals sign and values are required

  // If an equals sign was found, verify that it wasn't inside of quote marks.
  if (equals != value_end_) {
    for (std::string::const_iterator it = value_begin_; it != equals; ++it) {
      if (IsQuote(*it))
        return valid_ = false;  // Malformed, quote appears before equals sign
    }
  }

  name_begin_ = value_begin_;
  name_end_ = equals;
  value_begin_ = (equals == value_end_) ? value_end_ : equals + 1;

  TrimLWS(&name_begin_, &name_end_);
  TrimLWS(&value_begin_, &value_end_);
  value_is_quoted_ = false;
  unquoted_value_.clear();

  if (equals != value_end_ && value_begin_ == value_end_) {
    // Malformed; value is empty
    return valid_ = false;
  }

  if (value_begin_ != value_end_ && IsQuote(*value_begin_)) {
    value_is_quoted_ = true;

    if (strict_quotes_) {
      if (!HttpUtil::StrictUnquote(base::StringPiece(value_begin_, value_end_),
                                   &unquoted_value_))
        return valid_ = false;
      return true;
    }

    // Trim surrounding quotemarks off the value
    if (*value_begin_ != *(value_end_ - 1) || value_begin_ + 1 == value_end_) {
      // NOTE: This is not as graceful as it sounds:
      // * quoted-pairs will no longer be unquoted
      //   (["\"hello] should give ["hello]).
      // * Does not detect when the final quote is escaped
      //   (["value\"] should give [value"])
      value_is_quoted_ = false;
      ++value_begin_;  // Gracefully recover from mismatching quotes.
    } else {
      // Do not store iterators into this. See declaration of unquoted_value_.
      unquoted_value_ =
          HttpUtil::Unquote(base::StringPiece(value_begin_, value_end_));
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
    base::StringPiece entry = tokenizer.token_piece();
    entry = TrimLWS(entry);
    size_t semicolon_pos = entry.find(';');
    if (semicolon_pos == base::StringPiece::npos) {
      if (entry.find_first_of(HTTP_LWS) != base::StringPiece::npos)
        return false;
      allowed_encodings->insert(base::ToLowerASCII(entry));
      continue;
    }
    base::StringPiece encoding = entry.substr(0, semicolon_pos);
    encoding = TrimLWS(encoding);
    if (encoding.find_first_of(HTTP_LWS) != base::StringPiece::npos)
      return false;
    base::StringPiece params = entry.substr(semicolon_pos + 1);
    params = TrimLWS(params);
    size_t equals_pos = params.find('=');
    if (equals_pos == base::StringPiece::npos)
      return false;
    base::StringPiece param_name = params.substr(0, equals_pos);
    param_name = TrimLWS(param_name);
    if (!base::LowerCaseEqualsASCII(param_name, "q"))
      return false;
    base::StringPiece qvalue = params.substr(equals_pos + 1);
    qvalue = TrimLWS(qvalue);
    if (qvalue.empty())
      return false;
    if (qvalue[0] == '1') {
      if (base::StringPiece("1.000").starts_with(qvalue)) {
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
    base::StringPiece encoding = TrimLWS(encoding_tokenizer.token_piece());
    if (encoding.find_first_of(HTTP_LWS) != base::StringPiece::npos)
      return false;
    used_encodings->insert(base::ToLowerASCII(encoding));
  }
  return true;
}

}  // namespace net
