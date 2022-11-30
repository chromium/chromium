// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "media/cast/common/frame_id.h"

namespace media {
namespace cast {

// Enums used to indicate transport readiness state.
enum CastTransportStatus {
  TRANSPORT_STREAM_UNINITIALIZED = 0,
  TRANSPORT_STREAM_INITIALIZED,
  TRANSPORT_INVALID_CRYPTO_CONFIG,
  TRANSPORT_SOCKET_ERROR,
  CAST_TRANSPORT_STATUS_LAST = TRANSPORT_SOCKET_ERROR
};

// kRtcpCastAllPacketsLost is used in PacketIDSet and
// on the wire to mean that ALL packets for a particular
// frame are lost.
const uint16_t kRtcpCastAllPacketsLost = 0xffff;

// kRtcpCastLastPacket is used in PacketIDSet to ask for
// the last packet of a frame to be retransmitted.
const uint16_t kRtcpCastLastPacket = 0xfffe;

const size_t kMaxIpPacketSize = 1500;

// Each uint16_t represents one packet id within a cast frame.
// Can also contain kRtcpCastAllPacketsLost and kRtcpCastLastPacket.
using PacketIdSet = std::set<uint16_t>;

using MissingFramesAndPacketsMap = std::map<FrameId, PacketIdSet>;

using Packet = std::vector<uint8_t>;
using PacketRef = scoped_refptr<base::RefCountedData<Packet>>;
using PacketList = std::vector<PacketRef>;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_DEFINES_H_
