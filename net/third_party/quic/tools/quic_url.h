// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_URL_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_URL_H_

#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "url/gurl.h"

namespace quic {

// A utility class that wraps GURL.
class QuicUrl {
 public:
  // Constructs an empty QuicUrl.
  QuicUrl() = default;

  // Constructs a QuicUrl from the url string |url|.
  //
  // NOTE: If |url| doesn't have a scheme, it will have an empty scheme
  // field. If that's not what you want, use the QuicUrlImpl(url,
  // default_scheme) form below.
  explicit QuicUrl(QuicStringPiece url);

  // Constructs a QuicUrlImpl from |url|, assuming that the scheme for the URL
  // is |default_scheme| if there is no scheme specified in |url|.
  QuicUrl(QuicStringPiece url, QuicStringPiece default_scheme);

  // Returns false if the URL is not valid.
  bool IsValid() const;

  // Returns full text of the QuicUrl if it is valid. Return empty string
  // otherwise.
  QuicString ToString() const;

  // Returns host:port.
  // If the host is empty, it will return an empty string.
  // If the host is an IPv6 address, it will be bracketed.
  // If port is not present or is equal to default_port of scheme (e.g., port
  // 80 for HTTP), it won't be returned.
  QuicString HostPort() const;

  // Returns a string assembles path, parameters and query.
  QuicString PathParamsQuery() const;

  QuicString scheme() const;
  QuicString host() const;
  QuicString path() const;
  uint16_t port() const;

 private:
  GURL url_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_URL_H_
