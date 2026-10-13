// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_WEB_TRANSPORT_SEND_STREAM_STATS_H_
#define NET_QUIC_WEB_TRANSPORT_SEND_STREAM_STATS_H_

#include <cstdint>
#include <iosfwd>

#include "net/base/net_export.h"

namespace net {

struct NET_EXPORT WebTransportSendStreamStats final {
  uint64_t bytes_sent = 0;
  uint64_t bytes_acknowledged = 0;

  bool operator==(const WebTransportSendStreamStats&) const = default;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const WebTransportSendStreamStats& stats);

}  // namespace net

#endif  // NET_QUIC_WEB_TRANSPORT_SEND_STREAM_STATS_H_
