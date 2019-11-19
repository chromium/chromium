// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_HEADERS_H_
#define NET_HTTP_HTTP_RESPONSE_HEADERS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/http_version.h"
#include "net/log/net_log_capture_mode.h"

namespace base {
class Pickle;
class PickleIterator;
class Time;
class TimeDelta;
class Value;
}

namespace net {

class HttpByteRange;

enum ValidationType {
  VALIDATION_NONE,          // The resource is fresh.
  VALIDATION_ASYNCHRONOUS,  // The resource requires async revalidation.
  VALIDATION_SYNCHRONOUS    // The resource requires sync revalidation.
};

// HttpResponseHeaders: parses and holds HTTP response headers.
class NET_EXPORT HttpResponseHeaders
    : public base::RefCountedThreadSafe<HttpResponseHeaders> {
 public:
  // Persist options.
  typedef int PersistOptions;
  static const PersistOptions PERSIST_RAW = -1;  // Raw, unparsed headers.
  static const PersistOptions PERSIST_ALL = 0;  // Parsed headers.
  static const PersistOptions PERSIST_SANS_COOKIES = 1 << 0;
  static const PersistOptions PERSIST_SANS_CHALLENGES = 1 << 1;
  static const PersistOptions PERSIST_SANS_HOP_BY_HOP = 1 << 2;
  static const PersistOptions PERSIST_SANS_NON_CACHEABLE = 1 << 3;
  static const PersistOptions PERSIST_SANS_RANGES = 1 << 4;
  static const PersistOptions PERSIST_SANS_SECURITY_STATE = 1 << 5;

  struct FreshnessLifetimes {
    // How long the resource will be fresh for.
    base::TimeDelta freshness;
    // How long after becoming not fresh that the resource will be stale but
    // usable (if async revalidation is enabled).
    base::TimeDelta staleness;
  };

  static const char kContentRange[];

  HttpResponseHeaders() = delete;

  // Parses the given raw_headers.  raw_headers should be formatted thus:
  // includes the http status response line, each line is \0-terminated, and
  // it's terminated by an empty line (ie, 2 \0s in a row).
  // (Note that line continuations should have already been joined;
  // see HttpUtil::AssembleRawHeaders)
  //
  // HttpResponseHeaders does not perform any encoding changes on the input.
  //
  explicit HttpResponseHeaders(const std::string& raw_headers);

  // Initializes from the representation stored in the given pickle.  The data
  // for this object is found relative to the given pickle_iter, which should
  // be passed to the pickle's various Read* methods.
  explicit HttpResponseHeaders(base::PickleIterator* pickle_iter);

  // Takes headers as an ASCII string and tries to parse them as HTTP response
  // headers. returns nullptr on failure. Unlike the HttpResponseHeaders
  // constructor that takes a std::string, HttpUtil::AssembleRawHeaders should
  // not be called on |headers| before calling this method.
  static scoped_refptr<HttpResponseHeaders> TryToCreate(
      base::StringPiece headers);

  // Appends a representation of this object to the given pickle.
  // The options argument can be a combination of PersistOptions.
  void Persist(base::Pickle* pickle, PersistOptions options);

  // Performs header merging as described in 13.5.3 of RFC 2616.
  void Update(const HttpResponseHeaders& new_headers);

  // Removes all instances of a particular header.
  void RemoveHeader(const std::string& name);

  // Removes all instances of particular headers.
  void RemoveHeaders(const std::unordered_set<std::string>& header_names);

  // Removes a particular header line. The header name is compared
  // case-insensitively.
  void RemoveHeaderLine(const std::string& name, const std::string& value);

  // Adds a particular header.  |header| has to be a single header without any
  // EOL termination, just [<header-name>: <header-values>]
  // If a header with the same name is already stored, the two headers are not
  // merged together by this method; the one provided is simply put at the
  // end of the list.
  void AddHeader(const std::string& header);

  // Adds a cookie header. |cookie_string| should be the header value without
  // the header name (Set-Cookie).
  void AddCookie(const std::string& cookie_string);

  // Replaces the current status line with the provided one (|new_status| should
  // not have any EOL).
  void ReplaceStatusLine(const std::string& new_status);

  // Updates headers (Content-Length and Content-Range) in the |headers| to
  // include the right content length and range for |byte_range|.  This also
  // updates HTTP status line if |replace_status_line| is true.
  // |byte_range| must have a valid, bounded range (i.e. coming from a valid
  // response or should be usable for a response).
  void UpdateWithNewRange(const HttpByteRange& byte_range,
                          int64_t resource_size,
                          bool replace_status_line);

  // Fetch the "normalized" value of a single header, where all values for the
  // header name are separated by commas.  See the GetNormalizedHeaders for
  // format details.  Returns false if this header wasn't found.
  //
  // NOTE: Do not make any assumptions about the encoding of this output
  // string.  It may be non-ASCII, and the encoding used by the server is not
  // necessarily known to us.  Do not assume that this output is UTF-8!
  bool GetNormalizedHeader(const std::string& name, std::string* value) const;

  // Returns the normalized status line.
  std::string GetStatusLine() const;

  // Get the HTTP version of the normalized status line.
  HttpVersion GetHttpVersion() const {
    return http_version_;
  }

  // Get the HTTP status text of the normalized status line.
  std::string GetStatusText() const;

  // Enumerate the "lines" of the response headers.  This skips over the status
  // line.  Use GetStatusLine if you are interested in that.  Note that this
  // method returns the un-coalesced response header lines, so if a response
  // header appears on multiple lines, then it will appear multiple times in
  // this enumeration (in the order the header lines were received from the
  // server).  Also, a given header might have an empty value.  Initialize a
  // 'size_t' variable to 0 and pass it by address to EnumerateHeaderLines.
  // Call EnumerateHeaderLines repeatedly until it returns false.  The
  // out-params 'name' and 'value' are set upon success.
  bool EnumerateHeaderLines(size_t* iter,
                            std::string* name,
                            std::string* value) const;

  // Enumerate the values of the specified header.   If you are only interested
  // in the first header, then you can pass nullptr for the 'iter' parameter.
  // Otherwise, to iterate across all values for the specified header,
  // initialize a 'size_t' variable to 0 and pass it by address to
  // EnumerateHeader. Note that a header might have an empty value. Call
  // EnumerateHeader repeatedly until it returns false.
  //
  // Unless a header is explicitly marked as non-coalescing (see
  // HttpUtil::IsNonCoalescingHeader), headers that contain
  // comma-separated lists are treated "as if" they had been sent as
  // distinct headers. That is, a header of "Foo: a, b, c" would
  // enumerate into distinct values of "a", "b", and "c". This is also
  // true for headers that occur multiple times in a response; unless
  // they are marked non-coalescing, "Foo: a, b" followed by "Foo: c"
  // will enumerate to "a", "b", "c". Commas inside quoted strings are ignored,
  // for example a header of 'Foo: "a, b", "c"' would enumerate as '"a, b"',
  // '"c"'.
  //
  // This can cause issues for headers that might have commas in fields that
  // aren't quoted strings, for example a header of "Foo: <a, b>, <c>" would
  // enumerate as '<a', 'b>', '<c>', rather than as '<a, b>', '<c>'.
  //
  // To handle cases such as this, use GetNormalizedHeader to return the full
  // concatenated header, and then parse manually.
  bool EnumerateHeader(size_t* iter,
                       const base::StringPiece& name,
                       std::string* value) const;

  // Returns true if the response contains the specified header-value pair.
  // Both name and value are compared case insensitively.
  bool HasHeaderValue(const base::StringPiece& name,
                      const base::StringPiece& value) const;

  // Returns true if the response contains the specified header.
  // The name is compared case insensitively.
  bool HasHeader(const base::StringPiece& name) const;

  // Get the mime type and charset values in lower case form from the headers.
  // Empty strings are returned if the values are not present.
  void GetMimeTypeAndCharset(std::string* mime_type,
                             std::string* charset) const;

  // Get the mime type in lower case from the headers.  If there's no mime
  // type, returns false.
  bool GetMimeType(std::string* mime_type) const;

  // Get the charset in lower case from the headers.  If there's no charset,
  // returns false.
  bool GetCharset(std::string* charset) const;

  // Returns true if this response corresponds to a redirect.  The target
  // location of the redirect is optionally returned if location is non-null.
  bool IsRedirect(std::string* location) const;

  // Returns true if the HTTP response code passed in corresponds to a
  // redirect.
  static bool IsRedirectResponseCode(int response_code);

  // Returns VALIDATION_NONE if the response can be reused without
  // validation. VALIDATION_ASYNCHRONOUS means the response can be re-used, but
  // asynchronous revalidation must be performed. VALIDATION_SYNCHRONOUS means
  // that the result cannot be reused without revalidation.
  // The result is relative to the current_time parameter, which is
  // a parameter to support unit testing.  The request_time parameter indicates
  // the time at which the request was made that resulted in this response,
  // which was received at response_time.
  ValidationType RequiresValidation(const base::Time& request_time,
                                    const base::Time& response_time,
                                    const base::Time& current_time) const;

  // Calculates the amount of time the server claims the response is fresh from
  // the time the response was generated.  See section 13.2.4 of RFC 2616.  See
  // RequiresValidation for a description of the response_time parameter.  See
  // the definition of FreshnessLifetimes above for the meaning of the return
  // value.  See RFC 5861 section 3 for the definition of
  // stale-while-revalidate.
  FreshnessLifetimes GetFreshnessLifetimes(
      const base::Time& response_time) const;

  // Returns the age of the response.  See section 13.2.3 of RFC 2616.
  // See RequiresValidation for a description of this method's parameters.
  base::TimeDelta GetCurrentAge(const base::Time& request_time,
                                const base::Time& response_time,
                                const base::Time& current_time) const;

  // The following methods extract values from the response headers.  If a
  // value is not present, or is invalid, then false is returned.  Otherwise,
  // true is returned and the out param is assigned to the corresponding value.
  bool GetMaxAgeValue(base::TimeDelta* value) const;
  bool GetAgeValue(base::TimeDelta* value) const;
  bool GetDateValue(base::Time* value) const;
  bool GetLastModifiedValue(base::Time* value) const;
  bool GetExpiresValue(base::Time* value) const;
  bool GetStaleWhileRevalidateValue(base::TimeDelta* value) const;

  // Extracts the time value of a particular header.  This method looks for the
  // first matching header value and parses its value as a HTTP-date.
  bool GetTimeValuedHeader(const std::string& name, base::Time* result) const;

  // Determines if this response indicates a keep-alive connection.
  bool IsKeepAlive() const;

  // Returns true if this response has a strong etag or last-modified header.
  // See section 13.3.3 of RFC 2616.
  bool HasStrongValidators() const;

  // Returns true if this response has any validator (either a Last-Modified or
  // an ETag) regardless of whether it is strong or weak.  See section 13.3.3 of
  // RFC 2616.
  bool HasValidators() const;

  // Extracts the value of the Content-Length header or returns -1 if there is
  // no such header in the response.
  int64_t GetContentLength() const;

  // Extracts the value of the specified header or returns -1 if there is no
  // such header in the response.
  int64_t GetInt64HeaderValue(const std::string& header) const;

  // Extracts the values in a Content-Range header and returns true if all three
  // values are present and valid for a 206 response; otherwise returns false.
  // The following values will be outputted:
  // |*first_byte_position| = inclusive position of the first byte of the range
  // |*last_byte_position| = inclusive position of the last byte of the range
  // |*instance_length| = size in bytes of the object requested
  // If this method returns false, then all of the outputs will be -1.
  bool GetContentRangeFor206(int64_t* first_byte_position,
                             int64_t* last_byte_position,
                             int64_t* instance_length) const;

  // Returns true if the response is chunk-encoded.
  bool IsChunkEncoded() const;

  // Creates a Value for use with the NetLog containing the response headers.
  base::Value NetLogParams(NetLogCaptureMode capture_mode) const;

  // Returns the HTTP response code.  This is 0 if the response code text seems
  // to exist but could not be parsed.  Otherwise, it defaults to 200 if the
  // response code is not found in the raw headers.
  int response_code() const { return response_code_; }

  // Returns the raw header string.
  const std::string& raw_headers() const { return raw_headers_; }

  // Returns true if |name| is a cookie related header name. This is consistent
  // with |PERSIST_SANS_COOKIES|.
  static bool IsCookieResponseHeader(base::StringPiece name);

 private:
  friend class base::RefCountedThreadSafe<HttpResponseHeaders>;

  using HeaderSet = std::unordered_set<std::string>;

  // The members of this structure point into raw_headers_.
  struct ParsedHeader;
  typedef std::vector<ParsedHeader> HeaderList;

  ~HttpResponseHeaders();

  // Initializes from the given raw headers.
  void Parse(const std::string& raw_input);

  // Helper function for ParseStatusLine.
  // Tries to extract the "HTTP/X.Y" from a status line formatted like:
  //    HTTP/1.1 200 OK
  // with line_begin and end pointing at the begin and end of this line.  If the
  // status line is malformed, returns HttpVersion(0,0).
  static HttpVersion ParseVersion(std::string::const_iterator line_begin,
                                  std::string::const_iterator line_end);

  // Tries to extract the status line from a header block, given the first
  // line of said header block.  If the status line is malformed, we'll
  // construct a valid one.  Example input:
  //    HTTP/1.1 200 OK
  // with line_begin and end pointing at the begin and end of this line.
  // Output will be a normalized version of this.
  void ParseStatusLine(std::string::const_iterator line_begin,
                       std::string::const_iterator line_end,
                       bool has_headers);

  // Find the header in our list (case-insensitive) starting with parsed_ at
  // index |from|.  Returns string::npos if not found.
  size_t FindHeader(size_t from, const base::StringPiece& name) const;

  // Search the Cache-Control header for a directive matching |directive|. If
  // present, treat its value as a time offset in seconds, write it to |result|,
  // and return true.
  bool GetCacheControlDirective(const base::StringPiece& directive,
                                base::TimeDelta* result) const;

  // Add a header->value pair to our list.  If we already have header in our
  // list, append the value to it.
  void AddHeader(std::string::const_iterator name_begin,
                 std::string::const_iterator name_end,
                 std::string::const_iterator value_begin,
                 std::string::const_iterator value_end);

  // Add to parsed_ given the fields of a ParsedHeader object.
  void AddToParsed(std::string::const_iterator name_begin,
                   std::string::const_iterator name_end,
                   std::string::const_iterator value_begin,
                   std::string::const_iterator value_end);

  // Replaces the current headers with the merged version of |raw_headers| and
  // the current headers without the headers in |headers_to_remove|. Note that
  // |headers_to_remove| are removed from the current headers (before the
  // merge), not after the merge.
  void MergeWithHeaders(const std::string& raw_headers,
                        const HeaderSet& headers_to_remove);

  // Adds the values from any 'cache-control: no-cache="foo,bar"' headers.
  void AddNonCacheableHeaders(HeaderSet* header_names) const;

  // Adds the set of header names that contain cookie values.
  static void AddSensitiveHeaders(HeaderSet* header_names);

  // Adds the set of rfc2616 hop-by-hop response headers.
  static void AddHopByHopHeaders(HeaderSet* header_names);

  // Adds the set of challenge response headers.
  static void AddChallengeHeaders(HeaderSet* header_names);

  // Adds the set of cookie response headers.
  static void AddCookieHeaders(HeaderSet* header_names);

  // Adds the set of content range response headers.
  static void AddHopContentRangeHeaders(HeaderSet* header_names);

  // Adds the set of transport security state headers.
  static void AddSecurityStateHeaders(HeaderSet* header_names);

  // We keep a list of ParsedHeader objects.  These tell us where to locate the
  // header-value pairs within raw_headers_.
  HeaderList parsed_;

  // The raw_headers_ consists of the normalized status line (terminated with a
  // null byte) and then followed by the raw null-terminated headers from the
  // input that was passed to our constructor.  We preserve the input [*] to
  // maintain as much ancillary fidelity as possible (since it is sometimes
  // hard to tell what may matter down-stream to a consumer of XMLHttpRequest).
  // [*] The status line may be modified.
  std::string raw_headers_;

  // This is the parsed HTTP response code.
  int response_code_;

  // The normalized http version (consistent with what GetStatusLine() returns).
  HttpVersion http_version_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponseHeaders);
};

using ResponseHeadersCallback =
    base::Callback<void(scoped_refptr<const HttpResponseHeaders>)>;

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_HEADERS_H_
