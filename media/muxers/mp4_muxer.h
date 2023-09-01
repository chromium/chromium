// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_H_
#define MEDIA_MUXERS_MP4_MUXER_H_

#include <string>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class AudioParameters;

class MEDIA_EXPORT Mp4Muxer : public Muxer {
 public:
  // `audio_codec` should coincide with whatever is sent in OnEncodedAudio(),
  Mp4Muxer(AudioCodec audio_codec,
           bool has_video,
           bool has_audio,
           Muxer::WriteDataCB write_data_callback);

  Mp4Muxer(const Mp4Muxer&) = delete;
  Mp4Muxer& operator=(const Mp4Muxer&) = delete;
  ~Mp4Muxer() override;

  void SetMaximumDurationToForceDataOutput(base::TimeDelta interval) override;
  bool OnEncodedVideo(
      const VideoParameters& params,
      std::string encoded_data,
      std::string encoded_alpha,
      absl::optional<VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp,
      bool is_key_frame) override;
  bool OnEncodedAudio(
      const AudioParameters& params,
      std::string encoded_data,
      absl::optional<AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;

  void SetLiveAndEnabled(bool track_live_and_enabled, bool is_video) override;

  void Pause() override;
  void Resume() override;
  bool Flush() override;

 private:
  void MaybeForceFlush();
  void Reset();
  base::TimeTicks AdjustTimestamp(base::TimeTicks timestamp, bool audio);

  std::unique_ptr<Mp4MuxerDelegate> mp4_muxer_delegate_;

  base::TimeDelta max_data_output_interval_;
  base::TimeTicks start_or_last_flushed_timestamp_;

  // Keeps track of how long we're paused for, so we can modify incoming
  // timestamps.
  absl::optional<base::ElapsedTimer> elapsed_time_in_pause_;
  base::TimeDelta total_time_in_pause_;

  const bool has_video_;
  const bool has_audio_;

  // The arriving samples could be out of order, then we need to ensure
  // that sample to the Delegate is in order by dropping old one.
  base::TimeTicks latest_video_timestamp_{base::TimeTicks::Min()};
  base::TimeTicks latest_audio_timestamp_{base::TimeTicks::Min()};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_H_
