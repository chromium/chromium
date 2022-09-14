// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_SCOPED_AV_PACKET_H_
#define MEDIA_FFMPEG_SCOPED_AV_PACKET_H_

#include <memory>

#include "media/base/media_export.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVPacket;

namespace media {

// Like std::unique_ptr<AVPacket>, but makes sure packets are only ever
// allocated with av_packet_alloc() and freed with av_packet_free().
class MEDIA_EXPORT ScopedAVPacket {
 public:
  // Constructs an empty ScopedAVPacket.
  ScopedAVPacket();
  ~ScopedAVPacket();

  ScopedAVPacket(const ScopedAVPacket&) = delete;
  ScopedAVPacket& operator=(const ScopedAVPacket&) = delete;

  ScopedAVPacket(ScopedAVPacket&&);
  ScopedAVPacket& operator=(ScopedAVPacket&&);

  // Returns a ScopedAVPacket wrapping a packet allocated with
  // av_packet_alloc().
  static ScopedAVPacket Allocate();

  AVPacket* get() const { return packet_.get(); }
  explicit operator bool() const { return static_cast<bool>(packet_); }
  AVPacket& operator*() const { return *packet_; }
  AVPacket* operator->() const { return packet_.get(); }

 private:
  explicit ScopedAVPacket(AVPacket* raw_packet);

  std::unique_ptr<AVPacket, ScopedPtrAVFreePacket> packet_;
};

}  // namespace media

#endif  // MEDIA_FFMPEG_SCOPED_AV_PACKET_H_
