// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MUXER_H_
#define MEDIA_MUXERS_MUXER_H_

#include <string>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class AudioParameters;

// Interface for muxers that take encoded audio and/or video data and mux it
// into a container. The format of the container is implementation specific, as
// is how the output is delivered.
class MEDIA_EXPORT Muxer {
 public:
  // Defines the type of a callback to be called when a derived muxer
  // (e.g. WebmMuxer or Mp4Muxer) is ready to write a chunk of data.
  using WriteDataCB = base::RepeatingCallback<void(base::StringPiece)>;

  // Container for the parameters that muxer uses that is extracted from
  // VideoFrame.
  struct MEDIA_EXPORT VideoParameters {
    explicit VideoParameters(const VideoFrame& frame);
    VideoParameters(gfx::Size visible_rect_size,
                    double frame_rate,
                    VideoCodec codec,
                    absl::optional<gfx::ColorSpace> color_space);
    VideoParameters(const VideoParameters&);
    ~VideoParameters();
    gfx::Size visible_rect_size;
    double frame_rate;
    VideoCodec codec;
    absl::optional<gfx::ColorSpace> color_space;
  };

  virtual ~Muxer() = default;

  // Optional. If set, the muxer will output a chunk of data after `interval`
  // amount of time has passed. Calling `Pause()` will prevent the duration from
  // accumulating.
  // Implementations may have a minimum duration that `interval` will not be set
  // below.
  // TODO(crbug.com/1381323): consider if cluster output should be based on
  // media timestamps.
  virtual void SetMaximumDurationToForceDataOutput(
      base::TimeDelta interval) = 0;

  // Functions to add video and audio frames with `encoded_data.data()`.
  // `encoded_alpha` represents the encode output of alpha channel when
  // available, can be empty otherwise.
  // Returns true if the data is accepted by the muxer, false otherwise.
  virtual bool OnEncodedVideo(const VideoParameters& params,
                              std::string encoded_data,
                              std::string encoded_alpha,
                              base::TimeTicks timestamp,
                              bool is_key_frame) = 0;
  virtual bool OnEncodedAudio(const AudioParameters& params,
                              std::string encoded_data,
                              base::TimeTicks timestamp) = 0;

  // Call to handle mute and tracks getting disabled. If the track is not live
  // and enabled, input data will be ignored and black frames or silence will
  // be output instead.
  virtual void SetLiveAndEnabled(bool track_live_and_enabled,
                                 bool is_video) = 0;

  // Calling `Pause()` will cause the muxer to modify the timestamps of inputs,
  // setting them to the last received value before the pause. This effectively
  // removes the input data from the presentation, while still preserving it.
  // Once `Resume()` is called, new inputs will have timestamps starting from
  // the time that `Pause()` was called.
  virtual void Pause() = 0;
  virtual void Resume() = 0;

  // Drains and writes out all buffered frames and finalizes the output.
  // Returns true on success, false otherwise.
  virtual bool Flush() = 0;
};

}  // namespace media

#endif  // MEDIA_MUXERS_MUXER_H_
