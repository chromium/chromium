// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_BITRATE_SUGGESTER_H_
#define MEDIA_CAST_SENDER_VIDEO_BITRATE_SUGGESTER_H_

#include "media/cast/cast_config.h"

namespace media::cast {

class VideoBitrateSuggester {
 public:
  using GetVideoNetworkBandwidthCB = base::RepeatingCallback<int()>;

  VideoBitrateSuggester(const FrameSenderConfig& config,
                        GetVideoNetworkBandwidthCB get_bitrate_cb);
  VideoBitrateSuggester(VideoBitrateSuggester&& other) = delete;
  VideoBitrateSuggester& operator=(VideoBitrateSuggester&& other) = delete;
  VideoBitrateSuggester(const VideoBitrateSuggester&) = delete;
  VideoBitrateSuggester& operator=(const VideoBitrateSuggester&) = delete;
  ~VideoBitrateSuggester();

  void RecordShouldDropNextFrame(bool should_drop);

  int GetSuggestedBitrate();

 private:
  // NOTE: the exponential algorithm is currently undergoing an experiment
  // versus the legacy implementation.
  // TODO(https://issuetracker.google.com/302584587): determine if new algorithm
  // is more effective.
  void UpdateSuggestionUsingExponentialAlgorithm();
  void UpdateSuggestionUsingLinearAlgorithm();

  // The method for getting the recommended bitrate.
  GetVideoNetworkBandwidthCB get_bandwidth_cb_;

  // The minimum and maximum bitrates set from the config.
  const int min_bitrate_ = 0;
  const int max_bitrate_ = 0;

  // The suggested bitrate, factoring in frame drops.
  int suggested_bitrate_ = 0;

  // We keep track of how many frames get dropped in order to lower the video
  // bitrate when appropriate.
  int frames_requested_ = 0;
  int frames_dropped_ = 0;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_VIDEO_BITRATE_SUGGESTER_H_
