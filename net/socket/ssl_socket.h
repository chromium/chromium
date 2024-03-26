// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SOCKET_H_
#define NET_SOCKET_SSL_SOCKET_H_

#include <string_view>

#include "net/base/net_export.h"
#include "net/socket/stream_socket.h"

namespace net {

// SSLSocket interface defines method that are common between client
// and server SSL sockets.
class NET_EXPORT SSLSocket : public StreamSocket {
public:
 ~SSLSocket() override = default;

 // Exports data derived from the SSL master-secret (see RFC 5705).
 // If |has_context| is false, uses the no-context construction from the
 // RFC and |context| is ignored.  The call will fail with an error if
 // the socket is not connected or the SSL implementation does not
 // support the operation.
 virtual int ExportKeyingMaterial(std::string_view label,
                                  bool has_context,
                                  std::string_view context,
                                  unsigned char* out,
                                  unsigned int outlen) = 0;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SOCKET_H_
