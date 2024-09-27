// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MUXER_TIMESTAMP_ADAPTER_H_
#define MEDIA_MUXERS_MUXER_TIMESTAMP_ADAPTER_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"
#include "media/muxers/muxer.h"

namespace media {

// Adapter for muxers that emits incoming samples in monotonically increasing
// timestamp order, and covers MediaRecorder pause/resume behavior as well as
// tracks ending and enabledness.
// This class is thread-compatible.
class MEDIA_EXPORT MuxerTimestampAdapter {
 public:
  MuxerTimestampAdapter(std::unique_ptr<Muxer> muxer,
                        bool has_video,
                        bool has_audio);
  MuxerTimestampAdapter(const MuxerTimestampAdapter&) = delete;
  MuxerTimestampAdapter& operator=(const MuxerTimestampAdapter&) = delete;
  ~MuxerTimestampAdapter();

  // Functions to add video and audio frames with `encoded_data.data()`.
  // `encoded_alpha` represents the encode output of alpha channel when
  // available, can be empty otherwise.
  // Returns true if the data is accepted by the muxer, false otherwise.
  bool OnEncodedVideo(
      const Muxer::VideoParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<media::VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp);
  bool OnEncodedAudio(
      const AudioParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<media::AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp);

  // Call to handle mute and tracks getting disabled. If the track is not live
  // and enabled, input data will be ignored and black frames or silence will
  // be output instead.
  void SetLiveAndEnabled(bool track_live_and_enabled, bool is_video);

  // Calling `Pause()` will cause the muxer to modify the timestamps of inputs,
  // setting them to the last received value before the pause. This effectively
  // removes the input data from the presentation, while still preserving it.
  // Once `Resume()` is called, new inputs will have muxer timestamps starting
  // from the time that `Pause()` was called.
  void Pause();
  void Resume();

  // Drains and writes out all buffered frames and finalizes the output.
  // Returns true on success, false otherwise.
  bool Flush();

  Muxer* GetMuxerForTesting() const { return muxer_.get(); }

 private:
  friend class MuxerTimestampAdapterTestBase;

  // Structure for keeping encoded frames in internal queues.
  struct EncodedFrame {
    EncodedFrame();
    EncodedFrame(Muxer::EncodedFrame frame,
                 base::TimeTicks timestamp_minus_paused);
    EncodedFrame(EncodedFrame&&);
    ~EncodedFrame();
    Muxer::EncodedFrame frame;
    // Timestamp of frame minus the total time in pause at the time.
    base::TimeTicks timestamp_minus_paused;
  };

  // Adds all currently buffered frames to the muxer in timestamp order,
  // until the queues are depleted.
  void FlushQueues();
  // Flushes out frames to the muxer while ensuring monotonically increasing
  // timestamps.
  // Note that frames may still be around in the queues after this call. The
  // method stops flushing when timestamp monotonicity can't be guaranteed
  // anymore.
  bool PartiallyFlushQueues();
  // Flushes out the next frame in timestamp order from the queues. Returns true
  // on success and false on muxer failure.
  // Note: it's assumed that at least one video or audio frame is queued.
  bool FlushNextFrame();
  // Ensures a monotonically increasing timestamp sequence, despite
  // incoming non-monotonically increasing timestamps.
  base::TimeTicks UpdateLastTimestampAndGetNext(
      std::optional<base::TimeTicks>& last_timestamp,
      base::TimeTicks incoming_timestamp);

  // TODO(ajose): Change these when support is added for multiple tracks.
  // http://crbug.com/528523
  const bool has_video_;
  const bool has_audio_;
  bool has_seen_video_ = false;
  bool has_seen_audio_ = false;

  std::unique_ptr<Muxer> muxer_;
  std::optional<base::TimeTicks> last_video_timestamp_;
  std::optional<base::TimeTicks> last_audio_timestamp_;

  // The timestamp of the lowest timestamp audio or video sample, compensated
  // for the total time in pause at the time.
  base::TimeTicks first_timestamp_;

  // Variables to measure and accumulate, respectively, the time in pause state.
  std::optional<base::ElapsedTimer> elapsed_time_in_pause_;
  base::TimeDelta total_time_in_pause_;

  // Variables to track live and enabled state of audio and video.
  bool video_track_live_and_enabled_ = true;
  bool audio_track_live_and_enabled_ = true;

  // Maximum interval between data output callbacks (given frames arriving)
  base::TimeDelta max_data_output_interval_;

  // Last timestamp written into the muxer.
  base::TimeDelta last_timestamp_written_;

  // The following two queues hold frames to ensure that monotonically
  // increasing timestamps are stored in the resulting file without modifying
  // the timestamps.
  base::circular_deque<EncodedFrame> audio_frames_;
  // If muxing audio and video, this queue holds frames until the first audio
  // frame appears.
  base::circular_deque<EncodedFrame> video_frames_;
};

}  // namespace media

#endif  // MEDIA_MUXERS_MUXER_TIMESTAMP_ADAPTER_H_
