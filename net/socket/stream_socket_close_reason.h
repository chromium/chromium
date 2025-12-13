// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_CLOSE_REASON_H_
#define NET_SOCKET_STREAM_SOCKET_CLOSE_REASON_H_

#include <string_view>

namespace net {

// Represents why a stream socket is closed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(StreamSocketCloseReason)
enum class StreamSocketCloseReason {
  kUnspecified = 0,
  kCloseAllConnections = 1,
  kIpAddressChanged = 2,
  kSslConfigChanged = 3,
  kCannotUseTcpBasedProtocols = 4,
  kSpdySessionCreated = 5,
  kQuicSessionCreated = 6,
  kUsingExistingSpdySession = 7,
  kUsingExistingQuicSession = 8,
  kAbort = 9,
  kAttemptManagerDraining = 10,
  kMaxValue = kAttemptManagerDraining,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:StreamSocketCloseReason,tools/metrics/histograms/metadata/net/histograms.xml:StreamSocketCloseReason)

std::string_view StreamSocketCloseReasonToString(
    StreamSocketCloseReason reason);

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_CLOSE_REASON_H_
