// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/receiver/framer.h"

#include "base/logging.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

Framer::Framer(const base::TickClock* clock,
               RtpPayloadFeedback* incoming_payload_feedback,
               uint32_t ssrc,
               bool decoder_faster_than_max_frame_rate,
               int max_unacked_frames)
    : decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      cast_msg_builder_(clock,
                        incoming_payload_feedback,
                        this,
                        ssrc,
                        decoder_faster_than_max_frame_rate,
                        max_unacked_frames),
      waiting_for_key_(true),
      last_released_frame_(FrameId::first() - 1),
      newest_frame_id_(FrameId::first() - 1) {
  DCHECK(incoming_payload_feedback) << "Invalid argument";
}

Framer::~Framer() = default;

bool Framer::InsertPacket(const uint8_t* payload_data,
                          size_t payload_size,
                          const RtpCastHeader& rtp_header,
                          bool* duplicate) {
  *duplicate = false;

  if (rtp_header.is_key_frame && waiting_for_key_) {
    last_released_frame_ = rtp_header.frame_id - 1;
    waiting_for_key_ = false;
  }

  VLOG(1) << "InsertPacket frame:" << rtp_header.frame_id
          << " packet:" << static_cast<int>(rtp_header.packet_id)
          << " max packet:" << static_cast<int>(rtp_header.max_packet_id);

  if ((rtp_header.frame_id <= last_released_frame_) && !waiting_for_key_) {
    // Packet is too old.
    return false;
  }

  // Update the last received frame id.
  if (rtp_header.frame_id > newest_frame_id_) {
    newest_frame_id_ = rtp_header.frame_id;
  }

  // Insert packet.
  const auto it = frames_.find(rtp_header.frame_id);
  FrameBuffer* buffer;
  if (it == frames_.end()) {
    buffer = new FrameBuffer();
    frames_.insert(std::make_pair(rtp_header.frame_id,
                                  std::unique_ptr<FrameBuffer>(buffer)));
  } else {
    buffer = it->second.get();
  }
  if (!buffer->InsertPacket(payload_data, payload_size, rtp_header)) {
    VLOG(3) << "Packet already received, ignored: frame " << rtp_header.frame_id
            << ", packet " << rtp_header.packet_id;
    *duplicate = true;
    return false;
  }

  return buffer->Complete();
}

// This does not release the frame.
bool Framer::GetEncodedFrame(EncodedFrame* frame,
                             bool* next_frame,
                             bool* have_multiple_decodable_frames) {
  *have_multiple_decodable_frames = HaveMultipleDecodableFrames();

  // Find frame id.
  FrameBuffer* buffer = FindNextFrameForRelease();
  if (buffer) {
    // We have our next frame.
    *next_frame = true;
  } else {
    // Check if we can skip frames when our decoder is too slow.
    if (!decoder_faster_than_max_frame_rate_)
      return false;

    buffer = FindOldestDecodableFrame();
    if (!buffer)
      return false;
    *next_frame = false;
  }

  return buffer->AssembleEncodedFrame(frame);
}

void Framer::AckFrame(FrameId frame_id) {
  VLOG(2) << "ACK frame " << frame_id;
  cast_msg_builder_.CompleteFrameReceived(frame_id);
}

void Framer::ReleaseFrame(FrameId frame_id) {
  const auto it = frames_.begin();
  const bool skipped_old_frame = it->first < frame_id;
  frames_.erase(it, frames_.upper_bound(frame_id));
  last_released_frame_ = frame_id;
  if (skipped_old_frame)
    cast_msg_builder_.UpdateCastMessage();
}

bool Framer::TimeToSendNextCastMessage(base::TimeTicks* time_to_send) {
  return cast_msg_builder_.TimeToSendNextCastMessage(time_to_send);
}

void Framer::SendCastMessage() {
  cast_msg_builder_.UpdateCastMessage();
}

FrameBuffer* Framer::FindNextFrameForRelease() {
  for (const auto& entry : frames_) {
    if (entry.second->Complete() && IsNextFrameForRelease(*entry.second))
      return entry.second.get();
  }
  return nullptr;
}

FrameBuffer* Framer::FindOldestDecodableFrame() {
  for (const auto& entry : frames_) {
    if (entry.second->Complete() && IsDecodableFrame(*entry.second))
      return entry.second.get();
  }
  return nullptr;
}

bool Framer::HaveMultipleDecodableFrames() const {
  bool found_one = false;
  for (const auto& entry : frames_) {
    if (entry.second->Complete() && IsDecodableFrame(*entry.second)) {
      if (found_one)
        return true;  // Found another.
      else
        found_one = true;  // Found first one.  Continue search for another.
    }
  }
  return false;
}

bool Framer::Empty() const {
  return frames_.empty();
}

int Framer::NumberOfCompleteFrames() const {
  int count = 0;
  for (const auto& entry : frames_) {
    if (entry.second->Complete())
      ++count;
  }
  return count;
}

bool Framer::FrameExists(FrameId frame_id) const {
  return frames_.end() != frames_.find(frame_id);
}

void Framer::GetMissingPackets(FrameId frame_id,
                               bool last_frame,
                               PacketIdSet* missing_packets) const {
  const auto it = frames_.find(frame_id);
  if (it == frames_.end())
    return;

  it->second->GetMissingPackets(last_frame, missing_packets);
}

bool Framer::IsNextFrameForRelease(const FrameBuffer& buffer) const {
  if (waiting_for_key_ && !buffer.is_key_frame())
    return false;
  return (last_released_frame_ + 1) == buffer.frame_id();
}

bool Framer::IsDecodableFrame(const FrameBuffer& buffer) const {
  if (buffer.is_key_frame())
    return true;
  if (waiting_for_key_)
    return false;
  // Self-reference?
  if (buffer.last_referenced_frame_id() == buffer.frame_id())
    return true;

  // Current frame is not necessarily referencing the last frame.
  // Has the reference frame been released already?
  return buffer.last_referenced_frame_id() <= last_released_frame_;
}

}  // namespace cast
}  // namespace media
