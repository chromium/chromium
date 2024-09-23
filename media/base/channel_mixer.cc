// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_mixer.h"

#include <stddef.h>
#include <string.h>

#include "base/check_op.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_mixing_matrix.h"
#include "media/base/vector_math.h"

namespace media {

ChannelMixer::ChannelMixer(ChannelLayout input_layout,
                           int input_channels,
                           ChannelLayout output_layout,
                           int output_channels) {
  Initialize(input_layout, input_channels, output_layout, output_channels);
}

ChannelMixer::ChannelMixer(
    const AudioParameters& input, const AudioParameters& output) {
  Initialize(input.channel_layout(),
             input.channels(),
             output.channel_layout(),
             output.channels());
}

void ChannelMixer::Initialize(
    ChannelLayout input_layout, int input_channels,
    ChannelLayout output_layout, int output_channels) {
  // Create the transformation matrix
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  remapping_ = matrix_builder.CreateTransformationMatrix(&matrix_);
}

ChannelMixer::~ChannelMixer() = default;

void ChannelMixer::Transform(const AudioBus* input, AudioBus* output) {
  CHECK_EQ(input->frames(), output->frames());
  TransformPartial(input, input->frames(), output);
}

void ChannelMixer::TransformPartial(const AudioBus* input,
                                    int frame_count,
                                    AudioBus* output) {
  CHECK_EQ(matrix_.size(), static_cast<size_t>(output->channels()));
  CHECK_EQ(matrix_[0].size(), static_cast<size_t>(input->channels()));
  CHECK_LE(frame_count, input->frames());
  CHECK_LE(frame_count, output->frames());

  // Zero initialize |output| so we're accumulating from zero.
  output->ZeroFrames(frame_count);

  // If we're just remapping we can simply copy the correct input to output.
  if (remapping_) {
    for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
      for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
        float scale = matrix_[output_ch][input_ch];
        if (scale > 0) {
          DCHECK_EQ(scale, 1.0f);
          memcpy(output->channel(output_ch), input->channel(input_ch),
                 sizeof(*output->channel(output_ch)) * frame_count);
          break;
        }
      }
    }
    return;
  }

  for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
    for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
      float scale = matrix_[output_ch][input_ch];
      // Scale should always be positive.  Don't bother scaling by zero.
      DCHECK_GE(scale, 0);
      if (scale > 0) {
        vector_math::FMAC(input->channel(input_ch), scale, frame_count,
                          output->channel(output_ch));
      }
    }
  }
}

}  // namespace media
