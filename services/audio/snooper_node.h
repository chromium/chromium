// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SNOOPER_NODE_H_
#define SERVICES_AUDIO_SNOOPER_NODE_H_

#include <limits>
#include <memory>
#include <optional>

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_mixer.h"
#include "media/base/multi_channel_resampler.h"
#include "services/audio/delay_buffer.h"
#include "services/audio/loopback_group_member.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

// Thread-safe implementation of Snooper that records the audio from a
// GroupMember on one thread, and re-renders it to the desired output format on
// another thread. Since the data flow rates are known to be driven by different
// clocks (audio hardware clock versus system clock), the base::TimeTicks
// reference clock is used to detect drift and automatically correct for it to
// maintain proper synchronization.
//
// Throughout this class, there are sample counters (in terms of the input
// audio's sample rate) that are tracked/computed. They refer to the media
// timestamp of the audio flowing through specific parts of the processing
// pipeline: inbound from OnData() calls → through the delay buffer → through
// the resampler → and outbound via Render() calls:
//
//   write position:  The position of audio about to be written into the delay
//                    buffer. This is managed by OnData().
//   read position:   The position of audio about to be read from the delay
//                    buffer and pushed into the resampler. This is managed by
//                    ReadFromDelayBuffer().
//   output position: The position of the audio about to come out of the
//                    resampler. This is computed within Render(). Note that
//                    this is a "virtual" position since it is in terms of the
//                    input audio's sample count, but refers to audio about to
//                    be generated in the output format (with a possibly
//                    different sample rate).
//
// Note that the media timestamps represented by the "positions," as well as the
// surrounding math operations, might seem backwards; but they are not. This is
// because the inbound audio is from a source that pre-renders audio for playout
// in the near future, while the outbound audio is audio that would have been
// played-out in the recent past.
class SnooperNode final : public LoopbackGroupMember::Snooper {
 public:
  // Use sample counts as a precise measure of audio signal position and time
  // duration.
  using FrameTicks = int64_t;

  // Contruct a SnooperNode that buffers input of one format and renders output
  // in [possibly] another format.
  SnooperNode(const media::AudioParameters& input_params,
              const media::AudioParameters& output_params);

  SnooperNode(const SnooperNode&) = delete;
  SnooperNode& operator=(const SnooperNode&) = delete;

  ~SnooperNode() final;

  // GroupMember::Snooper implementation. Inserts more data into the delay
  // buffer.
  void OnData(const media::AudioBus& input_bus,
              base::TimeTicks reference_time,
              double volume) final;

  // Given the timing of recent OnData() calls and the |duration| of output that
  // would be requested in a call to Render(), determine the latest possible
  // |reference_time| for a Render() call that won't result in an underrun.
  // Returns std::nullopt while current conditions prohibit making a reliable
  // suggestion.
  std::optional<base::TimeTicks> SuggestLatestRenderTime(FrameTicks duration);

  // Renders more audio that was recorded from the GroupMember until
  // |output_bus| is filled, resampling and remixing the channels if necessary.
  // |reference_time| is used for detecting skip-ahead (i.e., a significant
  // forward jump in the reference time) and also to maintain synchronization
  // with the input.
  void Render(base::TimeTicks reference_time, media::AudioBus* output_bus);

 private:
  // Helper to store the new |correction_fps|, recompute the resampling I/O
  // ratio, and reconfigure the resampler with the new ratio.
  void UpdateCorrectionRate(int correction_fps);

  // Called by the MultiChannelResampler to acquire more data from the delay
  // buffer. This is invoked in the same call stack (and thread) as Render(),
  // zero or more times as data is needed by the resampler.
  void ReadFromDelayBuffer(int ignored, media::AudioBus* resampler_bus);

  // Input and output audio parameters.
  const media::AudioParameters input_params_;
  const media::AudioParameters output_params_;

  // Input and output AudioBus time durations, pre-computed from the input and
  // output AudioParameters.
  const base::TimeDelta input_bus_duration_;
  const base::TimeDelta output_bus_duration_;

  // The ratio between the input sampling rate and the output sampling rate. It
  // is "perfect" because it assumes no clock skew. Corrections are applied to
  // this to determine the actual resampler I/O ratio.
  const double perfect_io_ratio_;

  // Protects concurrent access to |buffer_| and the |write_position_| and
  // |write_reference_time_|. All other members are either read-only, or are not
  // accessed by multiple threads.
  base::Lock lock_;

  // Allows input data to be recorded and then read-back from any position
  // later (by the resampler).
  DelayBuffer buffer_;  // Guarded by |lock_|.

  // The next frame position at which to write into the delay buffer, and the
  // TimeTicks representing its corresponding system clock timestamp.
  FrameTicks write_position_;             // Guarded by |lock_|.
  base::TimeTicks write_reference_time_;  // Guarded by |lock_|.

  // Used by SuggestLatestRenderTime() to track whether OnData() has been called
  // recently, and as a basis for its suggestion. Other methods should not
  // depend on this value for anything.
  base::TimeTicks checkpoint_time_;

  // The next frame position from which to read from the delay buffer. This is
  // the position of the frames about to be pushed into the resampler, not the
  // position of frames about to be Render()'ed.
  FrameTicks read_position_;

  // The expected |reference_time| to be provided in the next call to Render().
  // This is used to detect skip-ahead in the output, and compensate when
  // necessary.
  base::TimeTicks render_reference_time_;

  // The additional number of frames currently being consumed by the resampler
  // each second to correct for drift.
  int correction_fps_;

  // Resamples input audio that is read from the delay buffer. Even if the input
  // and output have the same sampling rate, this is used to subtly stretch the
  // audio signal to correct for drift.
  media::MultiChannelResampler resampler_;

  // Specifies whether channel mixing should occur before or after resampling,
  // or is not needed. The strategy is chosen such that the minimal number of
  // channels are resampled, as resampling is the more-expensive operation.
  enum { kBefore, kAfter, kNone } const channel_mix_strategy_;

  // Only used when the input channel layout differs from the output.
  media::ChannelMixer channel_mixer_;

  // Only allocated when using the channel mixer. When using the kAfter
  // strategy, it is allocated just once, in the constructor, since its frame
  // length is constant. When using the kBefore strategy, it is re-allocated
  // whenever a larger one is needed and is reused thereafter.
  std::unique_ptr<media::AudioBus> mix_bus_;

  // An impossible value re-purposed to represent the "null" or "not set yet"
  // condition for |read_position_| and |write_position_|.
  static constexpr FrameTicks kNullPosition =
      std::numeric_limits<FrameTicks>::min();

  // The frame position where recording into the delay buffer always starts.
  static constexpr FrameTicks kWriteStartPosition = 0;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SNOOPER_NODE_H_
