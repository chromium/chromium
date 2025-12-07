// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SOCKET_H_
#define NET_SOCKET_SSL_SOCKET_H_

#include <string_view>

#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/socket/stream_socket.h"

namespace net {

// SSLSocket interface defines method that are common between client
// and server SSL sockets.
class NET_EXPORT SSLSocket : public StreamSocket {
 public:
  ~SSLSocket() override = default;

  // Exports data derived from the SSL master-secret (see RFC 5705).  The call
  // will fail with an error if the socket is not connected or the SSL
  // implementation does not support the operation. Note that |label| is
  // required (per RFC 5705 section 4) to be ASCII and subclasses enforce this
  // requirement.
  //
  // Note that in TLS < 1.3, passing std::nullopt for context produces a
  // different result from passing a populated option containing an empty span.
  // TLS 1.3 did away with this distinction and passing std::nullopt has the
  // same behavior as passing base::span(). See RFC 5705 section 4 for TLS <
  // 1.3 and RFC 8446 section 7.5 for TLS 1.3.
  //
  // Once we drop support for TLS < 1.3 (some day...) the context argument here
  // can cease being optional.
  virtual int ExportKeyingMaterial(
      std::string_view label,
      std::optional<base::span<const uint8_t>> context,
      base::span<uint8_t> out) = 0;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SOCKET_H_
