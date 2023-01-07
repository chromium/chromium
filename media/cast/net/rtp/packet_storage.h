// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTP_PACKET_STORAGE_H_
#define MEDIA_CAST_NET_RTP_PACKET_STORAGE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "media/cast/net/pacing/paced_sender.h"

namespace media {
namespace cast {

class PacketStorage {
 public:
  PacketStorage();

  PacketStorage(const PacketStorage&) = delete;
  PacketStorage& operator=(const PacketStorage&) = delete;

  virtual ~PacketStorage();

  // Store all of the packets for a frame.
  void StoreFrame(FrameId frame_id, const SendPacketVector& packets);

  // Release all of the packets for a frame.
  void ReleaseFrame(FrameId frame_id);

  // Returns a list of packets for a frame, or nullptr if not found.
  SendPacketVector* GetFramePackets(FrameId frame_id);

  // Get the number of stored frames.
  size_t GetNumberOfStoredFrames() const;

 private:
  base::circular_deque<SendPacketVector> frames_;
  FrameId first_frame_id_in_list_;

  // The number of frames whose packets have been released, but the entry in the
  // |frames_| queue has not yet been popped.
  size_t zombie_count_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTP_PACKET_STORAGE_H_
