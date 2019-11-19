// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/log/net_log_capture_mode.h"

namespace base {
class Value;
}

namespace net {

class NET_EXPORT HttpRequestHeaders {
 public:
  struct NET_EXPORT HeaderKeyValuePair {
    HeaderKeyValuePair();
    HeaderKeyValuePair(const base::StringPiece& key,
                       const base::StringPiece& value);

    std::string key;
    std::string value;
  };

  typedef std::vector<HeaderKeyValuePair> HeaderVector;

  class NET_EXPORT Iterator {
   public:
    explicit Iterator(const HttpRequestHeaders& headers);
    ~Iterator();

    // Advances the iterator to the next header, if any.  Returns true if there
    // is a next header.  Use name() and value() methods to access the resultant
    // header name and value.
    bool GetNext();

    // These two accessors are only valid if GetNext() returned true.
    const std::string& name() const { return curr_->key; }
    const std::string& value() const { return curr_->value; }

   private:
    bool started_;
    HttpRequestHeaders::HeaderVector::const_iterator curr_;
    const HttpRequestHeaders::HeaderVector::const_iterator end_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  static const char kGetMethod[];

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
  static const char kSecOriginPolicy[];
  static const char kTransferEncoding[];
  static const char kUserAgent[];

  HttpRequestHeaders();
  HttpRequestHeaders(const HttpRequestHeaders& other);
  HttpRequestHeaders(HttpRequestHeaders&& other);
  ~HttpRequestHeaders();

  HttpRequestHeaders& operator=(const HttpRequestHeaders& other);
  HttpRequestHeaders& operator=(HttpRequestHeaders&& other);

  bool IsEmpty() const { return headers_.empty(); }

  bool HasHeader(const base::StringPiece& key) const {
    return FindHeader(key) != headers_.end();
  }

  // Gets the first header that matches |key|.  If found, returns true and
  // writes the value to |out|.
  bool GetHeader(const base::StringPiece& key, std::string* out) const;

  // Clears all the headers.
  void Clear();

  // Sets the header value pair for |key| and |value|.  If |key| already exists,
  // then the header value is modified, but the key is untouched, and the order
  // in the vector remains the same.  When comparing |key|, case is ignored.
  // The caller must ensure that |key| passes HttpUtil::IsValidHeaderName() and
  // |value| passes HttpUtil::IsValidHeaderValue().
  void SetHeader(const base::StringPiece& key, const base::StringPiece& value);

  // Does the same as above but without internal DCHECKs for validations.
  void SetHeaderWithoutCheckForTesting(const base::StringPiece& key,
                                       const base::StringPiece& value) {
    SetHeaderInternal(key, value);
  }

  // Sets the header value pair for |key| and |value|, if |key| does not exist.
  // If |key| already exists, the call is a no-op.
  // When comparing |key|, case is ignored.
  void SetHeaderIfMissing(const base::StringPiece& key,
                          const base::StringPiece& value);

  // Removes the first header that matches (case insensitive) |key|.
  void RemoveHeader(const base::StringPiece& key);

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
  void AddHeaderFromString(const base::StringPiece& header_line);

  // Same thing as AddHeaderFromString() except that |headers| is a "\r\n"
  // delimited string of header lines.  It will split up the string by "\r\n"
  // and call AddHeaderFromString() on each.
  void AddHeadersFromString(const base::StringPiece& headers);

  // Calls SetHeader() on each header from |other|, maintaining order.
  void MergeFrom(const HttpRequestHeaders& other);

  // Copies from |other| to |this|.
  void CopyFrom(const HttpRequestHeaders& other) {
    *this = other;
  }

  void Swap(HttpRequestHeaders* other) {
    headers_.swap(other->headers_);
  }

  // Serializes HttpRequestHeaders to a string representation.  Joins all the
  // header keys and values with ": ", and inserts "\r\n" between each header
  // line, and adds the trailing "\r\n".
  std::string ToString() const;

  // Takes in the request line and returns a Value for use with the NetLog
  // containing both the request line and all headers fields.
  base::Value NetLogParams(const std::string& request_line,
                           NetLogCaptureMode capture_mode) const;

  const HeaderVector& GetHeaderVector() const { return headers_; }

 private:
  HeaderVector::iterator FindHeader(const base::StringPiece& key);
  HeaderVector::const_iterator FindHeader(const base::StringPiece& key) const;

  void SetHeaderInternal(const base::StringPiece& key,
                         const base::StringPiece& value);

  HeaderVector headers_;

  // Allow the copy construction and operator= to facilitate copying in
  // HttpRequestHeaders.
  // TODO(willchan): Investigate to see if we can remove the need to copy
  // HttpRequestHeaders.
  // DISALLOW_COPY_AND_ASSIGN(HttpRequestHeaders);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_HEADERS_H_
