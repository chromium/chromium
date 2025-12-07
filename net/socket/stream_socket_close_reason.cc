// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/stream_socket_close_reason.h"

#include <string_view>

namespace net {

std::string_view StreamSocketCloseReasonToString(
    StreamSocketCloseReason reason) {
  switch (reason) {
    case StreamSocketCloseReason::kUnspecified:
      return "Unspecified";
    case StreamSocketCloseReason::kCloseAllConnections:
      return "CloseAllConnections";
    case StreamSocketCloseReason::kIpAddressChanged:
      return "IpAddressChanged";
    case StreamSocketCloseReason::kSslConfigChanged:
      return "SslConfigChanged";
    case StreamSocketCloseReason::kCannotUseTcpBasedProtocols:
      return "CannotUseTcpBasedProtocols";
    case StreamSocketCloseReason::kSpdySessionCreated:
      return "SpdySessionCreated";
    case StreamSocketCloseReason::kQuicSessionCreated:
      return "QuicSessionCreated";
    case StreamSocketCloseReason::kUsingExistingSpdySession:
      return "UsingExistingSpdySession";
    case StreamSocketCloseReason::kUsingExistingQuicSession:
      return "UsingExistingQuicSession";
    case StreamSocketCloseReason::kAbort:
      return "Abort";
    case StreamSocketCloseReason::kAttemptManagerDraining:
      return "AttemptManagerDraining";
  }
}

}  // namespace net
