// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_BACKEND_RESPONSE_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_BACKEND_RESPONSE_H_

#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/tools/quic_url.h"

namespace quic {

// Container for HTTP response header/body pairs
// fetched by the QuicSimpleServerBackend
class QuicBackendResponse {
 public:
  // A ServerPushInfo contains path of the push request and everything needed in
  // comprising a response for the push request.
  struct ServerPushInfo {
    ServerPushInfo(QuicUrl request_url,
                   spdy::SpdyHeaderBlock headers,
                   spdy::SpdyPriority priority,
                   QuicString body);
    ServerPushInfo(const ServerPushInfo& other);

    QuicUrl request_url;
    spdy::SpdyHeaderBlock headers;
    spdy::SpdyPriority priority;
    QuicString body;
  };

  enum SpecialResponseType {
    REGULAR_RESPONSE,      // Send the headers and body like a server should.
    CLOSE_CONNECTION,      // Close the connection (sending the close packet).
    IGNORE_REQUEST,        // Do nothing, expect the client to time out.
    BACKEND_ERR_RESPONSE,  // There was an error fetching the response from
                           // the backend, for example as a TCP connection
                           // error.
    INCOMPLETE_RESPONSE,   // The server will act as if there is a non-empty
                           // trailer but it will not be sent, as a result, FIN
                           // will not be sent too.
  };
  QuicBackendResponse();

  QuicBackendResponse(const QuicBackendResponse& other) = delete;
  QuicBackendResponse& operator=(const QuicBackendResponse& other) = delete;

  ~QuicBackendResponse();

  SpecialResponseType response_type() const { return response_type_; }
  const spdy::SpdyHeaderBlock& headers() const { return headers_; }
  const spdy::SpdyHeaderBlock& trailers() const { return trailers_; }
  const QuicStringPiece body() const { return QuicStringPiece(body_); }

  void set_response_type(SpecialResponseType response_type) {
    response_type_ = response_type;
  }

  void set_headers(spdy::SpdyHeaderBlock headers) {
    headers_ = std::move(headers);
  }
  void set_trailers(spdy::SpdyHeaderBlock trailers) {
    trailers_ = std::move(trailers);
  }
  void set_body(QuicStringPiece body) {
    body_.assign(body.data(), body.size());
  }

 private:
  SpecialResponseType response_type_;
  spdy::SpdyHeaderBlock headers_;
  spdy::SpdyHeaderBlock trailers_;
  QuicString body_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_BACKEND_RESPONSE_H_