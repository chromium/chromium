// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRAME_STATS_H_
#define REMOTING_PROTOCOL_FRAME_STATS_H_

#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class VideoPacket;
class FrameStatsMessage;

namespace protocol {

struct HostFrameStats {
  HostFrameStats();
  HostFrameStats(const HostFrameStats&);
  ~HostFrameStats();

  // Extracts timing fields from the |packet|.
  static HostFrameStats GetForVideoPacket(const VideoPacket& packet);

  // Converts FrameStatsMessage protobuf message to HostFrameStats.
  static HostFrameStats FromFrameStatsMessage(const FrameStatsMessage& message);
  void ToFrameStatsMessage(FrameStatsMessage* message_out) const;

  // Frame Size.
  int frame_size {};

  // Set to null for frames that were not sent after a fresh input event.
  base::TimeTicks latest_event_timestamp;

  // Set to TimeDelta::Max() when unknown.
  base::TimeDelta capture_delay = base::TimeDelta::Max();
  base::TimeDelta encode_delay = base::TimeDelta::Max();
  base::TimeDelta capture_pending_delay = base::TimeDelta::Max();
  base::TimeDelta capture_overhead_delay = base::TimeDelta::Max();
  base::TimeDelta encode_pending_delay = base::TimeDelta::Max();
  base::TimeDelta send_pending_delay = base::TimeDelta::Max();
  base::TimeDelta rtt_estimate = base::TimeDelta::Max();
  int bandwidth_estimate_kbps = -1;
  uint32_t capturer_id = webrtc::DesktopCapturerId::kUnknown;
  int frame_quality = -1;
};

struct ClientFrameStats {
  ClientFrameStats();
  ClientFrameStats(const ClientFrameStats&);
  ~ClientFrameStats();
  ClientFrameStats& operator=(const ClientFrameStats&);

  base::TimeTicks time_received;
  base::TimeTicks time_decoded;
  base::TimeTicks time_rendered;
};

struct FrameStats {
  FrameStats();
  FrameStats(const FrameStats&);
  ~FrameStats();

  HostFrameStats host_stats;
  ClientFrameStats client_stats;
};

class FrameStatsConsumer {
 public:
  virtual void OnVideoFrameStats(const FrameStats& stats) = 0;
 protected:
  virtual ~FrameStatsConsumer() {}
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FRAME_STATS_H_
