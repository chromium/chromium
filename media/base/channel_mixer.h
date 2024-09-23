// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_MIXER_H_
#define MEDIA_BASE_CHANNEL_MIXER_H_

#include <vector>

#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;
class AudioParameters;

// ChannelMixer is for converting audio between channel layouts.  The conversion
// matrix is built upon construction and used during each Transform() call.  The
// algorithm works by generating a conversion matrix mapping each output channel
// to list of input channels.  The transform renders all of the output channels,
// with each output channel rendered according to a weighted sum of the relevant
// input channels as defined in the matrix.
class MEDIA_EXPORT ChannelMixer {
 public:
  // To mix two channels into one and preserve loudness, we must apply
  // (1 / sqrt(2)) gain to each.
  static constexpr float kHalfPower = 0.707106781186547524401f;

  ChannelMixer(ChannelLayout input_layout,
               int input_channels,
               ChannelLayout output_layout,
               int output_channels);
  ChannelMixer(const AudioParameters& input, const AudioParameters& output);

  ChannelMixer(const ChannelMixer&) = delete;
  ChannelMixer& operator=(const ChannelMixer&) = delete;

  ~ChannelMixer();

  // Transforms all channels from |input| into |output| channels.
  void Transform(const AudioBus* input, AudioBus* output);

  // Transforms all channels from |input| into |output| channels, for just the
  // initial part of the input. Callers can use this to avoid reallocating
  // AudioBuses, if the length of the data changes frequently for their use
  // case.
  void TransformPartial(const AudioBus* input,
                        int frame_count,
                        AudioBus* output);

 private:
  void Initialize(ChannelLayout input_layout, int input_channels,
                  ChannelLayout output_layout, int output_channels);

  // 2D matrix of output channels to input channels.
  std::vector< std::vector<float> > matrix_;

  // Optimization case for when we can simply remap the input channels to output
  // channels and don't need to do a multiply-accumulate loop over |matrix_|.
  bool remapping_;
};

}  // namespace media

#endif  // MEDIA_BASE_CHANNEL_MIXER_H_
