// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_H_
#define MEDIA_MUXERS_MP4_MUXER_H_

#include <string>

#include "base/sequence_checker.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/muxer.h"

namespace media {

class MEDIA_EXPORT Mp4Muxer : public Muxer {
 public:
  // `audio_codec` should coincide with whatever is sent in OnEncodedAudio().
  // If set, `max_data_output_interval` indicates the allowed maximum time for
  // data output into the delegate provided frames are provided.
  Mp4Muxer(AudioCodec audio_codec,
           bool has_video,
           bool has_audio,
           std::unique_ptr<Mp4MuxerDelegateInterface> delegate,
           std::optional<base::TimeDelta> max_data_output_interval);

  Mp4Muxer(const Mp4Muxer&) = delete;
  Mp4Muxer& operator=(const Mp4Muxer&) = delete;
  ~Mp4Muxer() override;

  // Muxer overrides.
  bool PutFrame(EncodedFrame frame,
                base::TimeDelta relative_timestamp) override;
  bool Flush() override;
  void OnVideoEnded() override;

 private:
  void MaybeForceFragmentFlush(base::TimeDelta media_relative_timestamp);
  void Reset();

  const std::unique_ptr<Mp4MuxerDelegateInterface> mp4_muxer_delegate_;

  base::TimeDelta max_data_output_interval_;

  // Tracks the start time of the current fragment to enforce periodic fragment
  // flushes. It's updated after each fragment flush or on the first frame.
  std::optional<base::TimeDelta> flush_origin_;
  bool seen_audio_ = false;
  bool video_ended_ = false;

  const bool has_video_;
  const bool has_audio_;

  AudioCodec audio_codec_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_H_
