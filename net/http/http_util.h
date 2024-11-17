// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_UTIL_H_
#define NET_HTTP_HTTP_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_version.h"
#include "url/gurl.h"
#include "url/origin.h"

// This is a macro to support extending this string literal at compile time.
// Please excuse me polluting your global namespace!
#define HTTP_LWS " \t"

namespace net {

class HttpResponseHeaders;

class NET_EXPORT HttpUtil {
 public:
  // Generates a request line that is used for text-based HTTP messaging.
  static std::string GenerateRequestLine(std::string_view method,
                                         const GURL& url,
                                         bool is_for_get_to_http_proxy);

  // Returns the absolute URL, to be used for the http request. This url is
  // made up of the protocol, host, [port], path, [query]. Everything else
  // is stripped (username, password, reference).
  static std::string SpecForRequest(const GURL& url);

  // Parses the value of a Content-Type header.  |mime_type|, |charset|, and
  // |had_charset| output parameters must be valid pointers.  |boundary| may be
  // nullptr.  |*mime_type| and |*charset| should be empty and |*had_charset|
  // false when called with the first Content-Type header value in a given
  // header list.
  //
  // ParseContentType() supports parsing multiple Content-Type headers in the
  // same header list.  For this operation, subsequent calls should pass in the
  // same |mime_type|, |charset|, and |had_charset| arguments without clearing
  // them.
  //
  // The resulting mime_type and charset values are normalized to lowercase.
  // The mime_type and charset output values are only modified if the
  // content_type_str contains a mime type and charset value, respectively.  If
  // |boundary| is not null, then |*boundary| will be assigned the (unquoted)
  // value of the boundary parameter, if any.
  static void ParseContentType(std::string_view content_type_str,
                               std::string* mime_type,
                               std::string* charset,
                               bool* had_charset,
                               std::string* boundary);

  // Parses the value of a "Range" header as defined in RFC 7233 Section 2.1.
  // https://tools.ietf.org/html/rfc7233#section-2.1
  // Returns false on failure.
  static bool ParseRangeHeader(const std::string& range_specifier,
                               std::vector<HttpByteRange>* ranges);

  // Extracts the values in a Content-Range header and returns true if all three
  // values are present and valid for a 206 response; otherwise returns false.
  // The following values will be outputted:
  // |*first_byte_position| = inclusive position of the first byte of the range
  // |*last_byte_position| = inclusive position of the last byte of the range
  // |*instance_length| = size in bytes of the object requested
  // If this method returns false, then all of the outputs will be -1.
  static bool ParseContentRangeHeaderFor206(std::string_view content_range_spec,
                                            int64_t* first_byte_position,
                                            int64_t* last_byte_position,
                                            int64_t* instance_length);

  // Parses a Retry-After header that is either an absolute date/time or a
  // number of seconds in the future. Interprets absolute times as relative to
  // |now|. If |retry_after_string| is successfully parsed and indicates a time
  // that is not in the past, fills in |*retry_after| and returns true;
  // otherwise, returns false.
  static bool ParseRetryAfterHeader(const std::string& retry_after_string,
                                    base::Time now,
                                    base::TimeDelta* retry_after);

  // Formats a time in the IMF-fixdate format defined by RFC 7231 (satisfying
  // its HTTP-date format).
  //
  // This behaves identically to the function in base/i18n/time_formatting.h. It
  // is reimplemented here since net/ cannot depend on base/i18n/.
  static std::string TimeFormatHTTP(base::Time time);

  // Returns true if the request method is "safe" (per section 4.2.1 of
  // RFC 7231).
  static bool IsMethodSafe(std::string_view method);

  // Returns true if the request method is idempotent (per section 4.2.2 of
  // RFC 7231).
  static bool IsMethodIdempotent(std::string_view method);

  // Returns true if it is safe to allow users and scripts to specify a header
  // with a given |name| and |value|.
  // See https://fetch.spec.whatwg.org/#forbidden-request-header.
  // Does not check header validity.
  static bool IsSafeHeader(std::string_view name, std::string_view value);

  // Returns true if |name| is a valid HTTP header name.
  static bool IsValidHeaderName(std::string_view name);

  // Returns false if |value| contains NUL or CRLF. This method does not perform
  // a fully RFC-2616-compliant header value validation.
  static bool IsValidHeaderValue(std::string_view value);

  // Multiple occurances of some headers cannot be coalesced into a comma-
  // separated list since their values are (or contain) unquoted HTTP-date
  // values, which may contain a comma (see RFC 2616 section 3.3.1).
  static bool IsNonCoalescingHeader(std::string_view name);

  // Return true if the character is HTTP "linear white space" (SP | HT).
  // This definition corresponds with the HTTP_LWS macro, and does not match
  // newlines.
  //
  // ALWAYS_INLINE to force inlining even when compiled with -Oz in Clang.
  ALWAYS_INLINE static bool IsLWS(char c) {
    constexpr std::string_view kWhiteSpaceCharacters(HTTP_LWS);
    // Clang performs this optimization automatically at -O3, but Android is
    // compiled at -Oz, so we need to do it by hand.
    static_assert(kWhiteSpaceCharacters == " \t");
    return c == ' ' || c == '\t';
  }

  // Trim HTTP_LWS chars from the beginning and end of the string.
  static void TrimLWS(std::string::const_iterator* begin,
                      std::string::const_iterator* end);
  static std::string_view TrimLWS(std::string_view string);

  // Whether the character is a valid |tchar| as defined in RFC 7230 Sec 3.2.6.
  static bool IsTokenChar(char c);
  // Whether the string is a valid |token| as defined in RFC 7230 Sec 3.2.6.
  static bool IsToken(std::string_view str);

  // Whether the character is a control character (CTL) as defined in RFC 5234
  // Appendix B.1.
  static inline bool IsControlChar(char c) {
    return (c >= 0x00 && c <= 0x1F) || c == 0x7F;
  }

  // Whether the string is a valid |parmname| as defined in RFC 5987 Sec 3.2.1.
  static bool IsParmName(std::string_view str);

  // RFC 2616 Sec 2.2:
  // quoted-string = ( <"> *(qdtext | quoted-pair ) <"> )
  // Unquote() strips the surrounding quotemarks off a string, and unescapes
  // any quoted-pair to obtain the value contained by the quoted-string.
  // If the input is not quoted, then it works like the identity function.
  static std::string Unquote(std::string_view str);

  // Similar to Unquote(), but additionally validates that the string being
  // unescaped actually is a valid quoted string. Returns false for an empty
  // string, a string without quotes, a string with mismatched quotes, and
  // a string with unescaped embeded quotes.
  [[nodiscard]] static bool StrictUnquote(std::string_view str,
                                          std::string* out);

  // The reverse of Unquote() -- escapes and surrounds with "
  static std::string Quote(std::string_view str);

  // Returns the start of the status line, or std::string::npos if no status
  // line was found. This allows for 4 bytes of junk to precede the status line
  // (which is what Mozilla does too).
  static size_t LocateStartOfStatusLine(base::span<const uint8_t> buf);

  // Returns index beyond the end-of-headers marker or std::string::npos if not
  // found.  RFC 2616 defines the end-of-headers marker as a double CRLF;
  // however, some servers only send back LFs (e.g., Unix-based CGI scripts
  // written using the ASIS Apache module).  This function therefore accepts the
  // pattern LF[CR]LF as end-of-headers (just like Mozilla). The first line of
  // |buf| is considered the status line, even if empty. The parameter |i| is
  // the offset within |buf| to begin searching from.
  static size_t LocateEndOfHeaders(base::span<const uint8_t> buf, size_t i = 0);

  // Same as |LocateEndOfHeaders|, but does not expect a status line, so can be
  // used on multi-part responses or HTTP/1.x trailers.  As a result, if |buf|
  // starts with a single [CR]LF,  it is considered an empty header list, as
  // opposed to an empty status line above a header list.
  static size_t LocateEndOfAdditionalHeaders(base::span<const uint8_t> buf,
                                             size_t i = 0);

  // Assemble "raw headers" in the format required by HttpResponseHeaders.
  // This involves normalizing line terminators, converting [CR]LF to \0 and
  // handling HTTP line continuations (i.e., lines starting with LWS are
  // continuations of the previous line). |buf| should end at the
  // end-of-headers marker as defined by LocateEndOfHeaders. If a \0 appears
  // within the headers themselves, it will be stripped. This is a workaround to
  // avoid later code from incorrectly interpreting it as a line terminator.
  //
  // TODO(crbug.com/40496844): Should remove or internalize this to
  //                         HttpResponseHeaders.
  static std::string AssembleRawHeaders(std::string_view buf);

  // Converts assembled "raw headers" back to the HTTP response format. That is
  // convert each \0 occurence to CRLF. This is used by DevTools.
  // Since all line continuations info is already lost at this point, the result
  // consists of status line and then one line for each header.
  static std::string ConvertHeadersBackToHTTPResponse(const std::string& str);

  // Given a comma separated ordered list of language codes, return an expanded
  // list by adding the base language from language-region pair if it doesn't
  // already exist. This increases the chances of language matching in many
  // cases as explained at this w3c doc:
  // https://www.w3.org/International/questions/qa-lang-priorities#langtagdetail
  // Note that we do not support Q values (e.g. ;q=0.9) in |language_prefs|.
  static std::string ExpandLanguageList(const std::string& language_prefs);

  // Given a comma separated ordered list of language codes, return
  // the list with a qvalue appended to each language.
  // The way qvalues are assigned is rather simple. The qvalue
  // starts with 1.0 and is decremented by 0.1 for each successive entry
  // in the list until it reaches 0.1. All the entries after that are
  // assigned the same qvalue of 0.1. Also, note that the 1st language
  // will not have a qvalue added because the absence of a qvalue implicitly
  // means q=1.0.
  //
  // When making a http request, this should be used to determine what
  // to put in Accept-Language header. If a comma separated list of language
  // codes *without* qvalue is sent, web servers regard all
  // of them as having q=1.0 and pick one of them even though it may not
  // be at the beginning of the list (see http://crbug.com/5899).
  static std::string GenerateAcceptLanguageHeader(
      const std::string& raw_language_list);

  // Returns true if the parameters describe a response with a strong etag or
  // last-modified header.  See section 13.3.3 of RFC 2616.
  //
  // Non-nullopt times will be converted to std::strings and parsed, which can
  // be somewhat expensive.
  //
  // Note that HasStringValidators() being true for a set of headers implies
  // HasValidators() is also true.
  static bool HasStrongValidators(
      HttpVersion version,
      std::optional<std::string_view> etag_header,
      std::optional<std::string_view> last_modified_header,
      std::optional<std::string_view> date_header);

  // Returns true if this response has any validator (either a Last-Modified or
  // an ETag) regardless of whether it is strong or weak.  See section 13.3.3 of
  // RFC 2616.
  //
  // Non-nullopt times will be converted to std::strings and parsed, which can
  // be somewhat expensive.
  static bool HasValidators(
      HttpVersion version,
      std::optional<std::string_view> etag_header,
      std::optional<std::string_view> last_modified_header);

  // Gets a vector of common HTTP status codes for histograms of status
  // codes.  Currently returns everything in the range [100, 600), plus 0
  // (for invalid responses/status codes).
  static std::vector<int> GetStatusCodesForHistogram();

  // Maps an HTTP status code to one of the status codes in the vector
  // returned by GetStatusCodesForHistogram.
  static int MapStatusCodeForHistogram(int code);

  // Returns true if |accept_encoding| is well-formed.  Parsed encodings turned
  // to lower case, are placed to provided string-set. Resulting set is
  // augmented to fulfill the RFC 2616 and RFC 7231 recommendations, e.g. if
  // there is no encodings specified, then {"*"} is returned to denote that
  // client has to encoding preferences (but it does not imply that the
  // user agent will be able to correctly process all encodings).
  static bool ParseAcceptEncoding(const std::string& accept_encoding,
                                  std::set<std::string>* allowed_encodings);

  // Returns true if |content_encoding| is well-formed.  Parsed encodings turned
  // to lower case, are placed to provided string-set. See sections 14.11 and
  // 3.5 of RFC 2616.
  static bool ParseContentEncoding(const std::string& content_encoding,
                                   std::set<std::string>* used_encodings);

  // Return true if `headers` contain multiple `field_name` fields with
  // different values.
  static bool HeadersContainMultipleCopiesOfField(
      const HttpResponseHeaders& headers,
      const std::string& field_name);

  // Used to iterate over the name/value pairs of HTTP headers.  To iterate
  // over the values in a multi-value header, use ValuesIterator.
  // See AssembleRawHeaders for joining line continuations (this iterator
  // does not expect any).
  class NET_EXPORT HeadersIterator {
   public:
    HeadersIterator(std::string::const_iterator headers_begin,
                    std::string::const_iterator headers_end,
                    const std::string& line_delimiter);
    ~HeadersIterator();

    // Advances the iterator to the next header, if any.  Returns true if there
    // is a next header.  Use name* and values* methods to access the resultant
    // header name and values.
    bool GetNext();

    // Iterates through the list of headers, starting with the current position
    // and looks for the specified header.  Note that the name _must_ be
    // lower cased.
    // If the header was found, the return value will be true and the current
    // position points to the header.  If the return value is false, the
    // current position will be at the end of the headers.
    bool AdvanceTo(const char* lowercase_name);

    void Reset() {
      lines_.Reset();
    }

    std::string::const_iterator name_begin() const {
      return name_begin_;
    }
    std::string::const_iterator name_end() const {
      return name_end_;
    }
    std::string name() const {
      return std::string(name_begin_, name_end_);
    }
    std::string_view name_piece() const {
      return base::MakeStringPiece(name_begin_, name_end_);
    }

    std::string::const_iterator values_begin() const {
      return values_begin_;
    }
    std::string::const_iterator values_end() const {
      return values_end_;
    }
    std::string values() const {
      return std::string(values_begin_, values_end_);
    }
    std::string_view values_piece() const {
      return base::MakeStringPiece(values_begin_, values_end_);
    }

   private:
    base::StringTokenizer lines_;
    std::string::const_iterator name_begin_;
    std::string::const_iterator name_end_;
    std::string::const_iterator values_begin_;
    std::string::const_iterator values_end_;
  };

  // Iterates over delimited values in an HTTP header.  HTTP LWS is
  // automatically trimmed from the resulting values.
  //
  // When using this class to iterate over response header values, be aware that
  // for some headers (e.g., Last-Modified), commas are not used as delimiters.
  // This iterator should be avoided for headers like that which are considered
  // non-coalescing (see IsNonCoalescingHeader).
  //
  // This iterator is careful to skip over delimiters found inside an HTTP
  // quoted string.
  class NET_EXPORT ValuesIterator {
   public:
    ValuesIterator(std::string_view values,
                   char delimiter,
                   bool ignore_empty_values = true);

    ValuesIterator(const ValuesIterator& other);
    ~ValuesIterator();

    // Advances the iterator to the next value, if any.  Returns true if there
    // is a next value.  Use value* methods to access the resultant value.
    bool GetNext();

    std::string_view value() const { return value_; }

   private:
    base::StringViewTokenizer values_;
    std::string_view value_;
    bool ignore_empty_values_;
  };

  // Iterates over a delimited sequence of name-value pairs in an HTTP header.
  // Each pair consists of a token (the name), an equals sign, and either a
  // token or quoted-string (the value). Arbitrary HTTP LWS is permitted outside
  // of and between names, values, and delimiters.
  //
  // String iterators returned from this class' methods may be invalidated upon
  // calls to GetNext() or after the NameValuePairsIterator is destroyed.
  class NET_EXPORT NameValuePairsIterator {
   public:
    // Whether or not values are optional. Values::NOT_REQUIRED allows
    // e.g. name1=value1;name2;name3=value3, whereas Vaues::REQUIRED
    // will treat it as a parse error because name2 does not have a
    // corresponding equals sign.
    enum class Values { NOT_REQUIRED, REQUIRED };

    // Whether or not unmatched quotes should be considered a failure. By
    // default this class is pretty lenient and does a best effort to parse
    // values with mismatched quotes. When set to STRICT_QUOTES a value with
    // mismatched or otherwise invalid quotes is considered a parse error.
    enum class Quotes { STRICT_QUOTES, NOT_STRICT };

    NameValuePairsIterator(std::string_view value,
                           char delimiter,
                           Values optional_values = Values::REQUIRED,
                           Quotes strict_quotes = Quotes::NOT_STRICT);

    NameValuePairsIterator(const NameValuePairsIterator& other);

    ~NameValuePairsIterator();

    // Advances the iterator to the next pair, if any.  Returns true if there
    // is a next pair. Returns false on completion or on error. In the error
    // case, `valid()` will return false. Once GetNext() returns false, whether
    // due to error or completion, it should not be called again. Use name() and
    // value() methods to access the resultant value.
    //
    // Each call will invalidate the string views obtained through the previous
    // GetNext() call, as they may point to temporary buffers.
    bool GetNext();

    // Returns false if there was a parse error.
    bool valid() const { return valid_; }

    // The name of the current name-value pair.
    std::string_view name() const { return name_; }

    // The value of the current name-value pair. Note that the returned
    // string_view will be invalidated by the next GetNext() call.
    std::string_view value() const LIFETIME_BOUND {
      return value_is_quoted_ ? unquoted_value_ : value_;
    }

    bool value_is_quoted() const { return value_is_quoted_; }

    // The value before unquoting (if any).
    std::string_view raw_value() const LIFETIME_BOUND { return value_; }

   private:
    // Attempts to parse `name_value_pair`, populating `name_`, `value_`, and
    // `unquoted_value_`. returns false on failure. On failure, the caller
    // should clear those values, to ensure consistent behavior.
    bool ParseNameValuePair(std::string_view name_value_pair);

    HttpUtil::ValuesIterator props_;
    bool valid_ = true;

    std::string_view name_;
    std::string_view value_;

    // Do not store iterators into this string. The NameValuePairsIterator
    // is copyable/assignable, and if copied the copy's iterators would point
    // into the original's unquoted_value_ member.
    std::string unquoted_value_;

    bool value_is_quoted_ = false;

    // True if values are required for each name/value pair; false if a
    // name is permitted to appear without a corresponding value.
    bool values_optional_;

    // True if quotes values are required to be properly quoted; false if
    // mismatched quotes and other problems with quoted values should be more
    // or less gracefully treated as valid.
    bool strict_quotes_;
  };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_UTIL_H_
