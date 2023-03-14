// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpRequestHeaders manages the request headers.
// It maintains these in a vector of header key/value pairs, thereby maintaining
// the order of the headers.  This means that any lookups are linear time
// operations.

#ifndef NET_HTTP_HTTP_REQUEST_HEADERS_H_
#define NET_HTTP_HTTP_REQUEST_HEADERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/filter/source_stream.h"
#include "net/log/net_log_capture_mode.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {

class NET_EXPORT HttpRequestHeaders {
 public:
  struct NET_EXPORT HeaderKeyValuePair {
    HeaderKeyValuePair();
    HeaderKeyValuePair(base::StringPiece key, base::StringPiece value);
    HeaderKeyValuePair(base::StringPiece key, std::string&& value);
    // Inline to take advantage of the base::StringPiece constructor being
    // constexpr.
    HeaderKeyValuePair(base::StringPiece key, const char* value)
        : HeaderKeyValuePair(key, base::StringPiece(value)) {}

    std::string key;
    std::string value;
  };

  typedef std::vector<HeaderKeyValuePair> HeaderVector;

  class NET_EXPORT Iterator {
   public:
    explicit Iterator(const HttpRequestHeaders& headers);

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    ~Iterator();

    // Advances the iterator to the next header, if any.  Returns true if there
    // is a next header.  Use name() and value() methods to access the resultant
    // header name and value.
    bool GetNext();

    // These two accessors are only valid if GetNext() returned true.
    const std::string& name() const { return curr_->key; }
    const std::string& value() const { return curr_->value; }

   private:
    bool started_ = false;
    HttpRequestHeaders::HeaderVector::const_iterator curr_;
    const HttpRequestHeaders::HeaderVector::const_iterator end_;
  };

  static const char kConnectMethod[];
  static const char kDeleteMethod[];
  static const char kGetMethod[];
  static const char kHeadMethod[];
  static const char kOptionsMethod[];
  static const char kPatchMethod[];
  static const char kPostMethod[];
  static const char kPutMethod[];
  static const char kTraceMethod[];
  static const char kTrackMethod[];

  static const char kAccept[];
  static const char kAcceptCharset[];
  static const char kAcceptEncoding[];
  static const char kAcceptLanguage[];
  static const char kAuthorization[];
  static const char kCacheControl[];
  static const char kConnection[];
  static const char kContentType[];
  static const char kCookie[];
  static const char kContentLength[];
  static const char kHost[];
  static const char kIfMatch[];
  static const char kIfModifiedSince[];
  static const char kIfNoneMatch[];
  static const char kIfRange[];
  static const char kIfUnmodifiedSince[];
  static const char kOrigin[];
  static const char kPragma[];
  static const char kProxyAuthorization[];
  static const char kProxyConnection[];
  static const char kRange[];
  static const char kReferer[];
  static const char kTransferEncoding[];
  static const char kUserAgent[];

  HttpRequestHeaders();
  HttpRequestHeaders(const HttpRequestHeaders& other);
  HttpRequestHeaders(HttpRequestHeaders&& other);
  ~HttpRequestHeaders();

  HttpRequestHeaders& operator=(const HttpRequestHeaders& other);
  HttpRequestHeaders& operator=(HttpRequestHeaders&& other);

  bool IsEmpty() const { return headers_.empty(); }

  bool HasHeader(base::StringPiece key) const {
    return FindHeader(key) != headers_.end();
  }

  // Gets the first header that matches |key|.  If found, returns true and
  // writes the value to |out|.
  bool GetHeader(base::StringPiece key, std::string* out) const;

  // Clears all the headers.
  void Clear();

  // Sets the header value pair for |key| and |value|.  If |key| already exists,
  // then the header value is modified, but the key is untouched, and the order
  // in the vector remains the same.  When comparing |key|, case is ignored.
  // The caller must ensure that |key| passes HttpUtil::IsValidHeaderName() and
  // |value| passes HttpUtil::IsValidHeaderValue().
  void SetHeader(base::StringPiece key, base::StringPiece value);
  void SetHeader(base::StringPiece key, std::string&& value);
  // Inline to take advantage of the base::StringPiece constructor being
  // constexpr.
  void SetHeader(base::StringPiece key, const char* value) {
    SetHeader(key, base::StringPiece(value));
  }

  // Does the same as above but without internal DCHECKs for validations.
  void SetHeaderWithoutCheckForTesting(base::StringPiece key,
                                       base::StringPiece value);

  // Sets the header value pair for |key| and |value|, if |key| does not exist.
  // If |key| already exists, the call is a no-op.
  // When comparing |key|, case is ignored.
  //
  // The caller must ensure that |key| passes HttpUtil::IsValidHeaderName() and
  // |value| passes HttpUtil::IsValidHeaderValue().
  void SetHeaderIfMissing(base::StringPiece key, base::StringPiece value);

  // Removes the first header that matches (case insensitive) |key|.
  void RemoveHeader(base::StringPiece key);

  // Parses the header from a string and calls SetHeader() with it.  This string
  // should not contain any CRLF.  As per RFC7230 Section 3.2, the format is:
  //
  // header-field   = field-name ":" OWS field-value OWS
  //
  // field-name     = token
  // field-value    = *( field-content / obs-fold )
  // field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
  // field-vchar    = VCHAR / obs-text
  //
  // obs-fold       = CRLF 1*( SP / HTAB )
  //                ; obsolete line folding
  //                ; see Section 3.2.4
  //
  // AddHeaderFromString() will trim any LWS surrounding the
  // field-content.
  void AddHeaderFromString(base::StringPiece header_line);

  // Same thing as AddHeaderFromString() except that |headers| is a "\r\n"
  // delimited string of header lines.  It will split up the string by "\r\n"
  // and call AddHeaderFromString() on each.
  void AddHeadersFromString(base::StringPiece headers);

  // Calls SetHeader() on each header from |other|, maintaining order.
  void MergeFrom(const HttpRequestHeaders& other);

  // Copies from |other| to |this|.
  void CopyFrom(const HttpRequestHeaders& other) { *this = other; }

  void Swap(HttpRequestHeaders* other) { headers_.swap(other->headers_); }

  // Serializes HttpRequestHeaders to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n".
  std::string ToString() const;

  // Takes in the request line and returns a Value for use with the NetLog
  // containing both the request line and all headers fields.
  base::Value::Dict NetLogParams(const std::string& request_line,
                                 NetLogCaptureMode capture_mode) const;

  const HeaderVector& GetHeaderVector() const { return headers_; }

  // Sets Accept-Encoding header based on `url` and `accepted_stream_types`, if
  // it does not exist. "br" is appended only when `enable_brotli` is true.
  void SetAcceptEncodingIfMissing(
      const GURL& url,
      const absl::optional<base::flat_set<SourceStream::SourceType>>&
          accepted_stream_types,
      bool enable_brotli);

 private:
  HeaderVector::iterator FindHeader(base::StringPiece key);
  HeaderVector::const_iterator FindHeader(base::StringPiece key) const;

  void SetHeaderInternal(base::StringPiece key, std::string&& value);

  HeaderVector headers_;

  // Allow the copy construction and operator= to facilitate copying in
  // HttpRequestHeaders.
  // TODO(willchan): Investigate to see if we can remove the need to copy
  // HttpRequestHeaders.
  // DISALLOW_COPY_AND_ASSIGN(HttpRequestHeaders);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_HEADERS_H_
