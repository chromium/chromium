// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/packet_storage.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

PacketStorage::PacketStorage() : zombie_count_(0) {}

PacketStorage::~PacketStorage() = default;

size_t PacketStorage::GetNumberOfStoredFrames() const {
  return frames_.size() - zombie_count_;
}

void PacketStorage::StoreFrame(FrameId frame_id,
                               const SendPacketVector& packets) {
  if (packets.empty()) {
    NOTREACHED();
    return;
  }

  if (frames_.empty()) {
    first_frame_id_in_list_ = frame_id;
  } else {
    // Make sure frame IDs are consecutive.
    DCHECK_EQ(first_frame_id_in_list_ + frames_.size(), frame_id);
    // Make sure we aren't being asked to store more frames than the system's
    // design limit.
    DCHECK_LT(frames_.size(), static_cast<size_t>(kMaxUnackedFrames));
  }

  // Save new frame to the end of the list.
  frames_.push_back(packets);
}

void PacketStorage::ReleaseFrame(FrameId frame_id) {
  SendPacketVector* const packets = GetFramePackets(frame_id);
  if (!packets)
    return;

  packets->clear();
  ++zombie_count_;

  while (!frames_.empty() && frames_.front().empty()) {
    DCHECK_GT(zombie_count_, 0u);
    --zombie_count_;
    frames_.pop_front();
    ++first_frame_id_in_list_;
  }
}

SendPacketVector* PacketStorage::GetFramePackets(FrameId frame_id) {
  if (first_frame_id_in_list_.is_null())
    return nullptr;
  const int64_t offset = frame_id - first_frame_id_in_list_;
  if (offset < 0 || offset >= static_cast<int64_t>(frames_.size()) ||
      frames_[offset].empty()) {
    return nullptr;
  }
  return &frames_[offset];
}

}  // namespace cast
}  // namespace media
