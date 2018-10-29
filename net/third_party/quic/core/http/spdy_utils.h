// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_SPDY_UTILS_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_SPDY_UTILS_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "net/third_party/quic/core/http/quic_header_list.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/spdy/core/spdy_framer.h"

namespace quic {

class QUIC_EXPORT_PRIVATE SpdyUtils {
 public:
  SpdyUtils() = delete;

  // Populate |content length| with the value of the content-length header.
  // Returns true on success, false if parsing fails or content-length header is
  // missing.
  static bool ExtractContentLengthFromHeaders(int64_t* content_length,
                                              spdy::SpdyHeaderBlock* headers);

  // Copies a list of headers to a SpdyHeaderBlock.
  static bool CopyAndValidateHeaders(const QuicHeaderList& header_list,
                                     int64_t* content_length,
                                     spdy::SpdyHeaderBlock* headers);

  // Copies a list of headers to a SpdyHeaderBlock.
  static bool CopyAndValidateTrailers(const QuicHeaderList& header_list,
                                      size_t* final_byte_offset,
                                      spdy::SpdyHeaderBlock* trailers);

  // Returns a canonicalized URL composed from the :scheme, :authority, and
  // :path headers of a PUSH_PROMISE. Returns empty string if the headers do not
  // conform to HTTP/2 spec or if the ":method" header contains a forbidden
  // method for PUSH_PROMISE.
  static QuicString GetPromisedUrlFromHeaders(
      const spdy::SpdyHeaderBlock& headers);

  // Returns hostname, or empty string if missing.
  static QuicString GetPromisedHostNameFromHeaders(
      const spdy::SpdyHeaderBlock& headers);

  // Returns true if result of |GetPromisedUrlFromHeaders()| is non-empty
  // and is a well-formed URL.
  static bool PromisedUrlIsValid(const spdy::SpdyHeaderBlock& headers);

  // Populates the fields of |headers| to make a GET request of |url|,
  // which must be fully-qualified.
  static bool PopulateHeaderBlockFromUrl(const QuicString url,
                                         spdy::SpdyHeaderBlock* headers);

  // Returns a canonical, valid URL for a PUSH_PROMISE with the specified
  // ":scheme", ":authority", and ":path" header fields, or an empty
  // string if the resulting URL is not valid or supported.
  static QuicString GetPushPromiseUrl(QuicStringPiece scheme,
                                      QuicStringPiece authority,
                                      QuicStringPiece path);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_SPDY_UTILS_H_
