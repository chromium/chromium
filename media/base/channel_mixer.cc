// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

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

  if (frame_count <= 0) {
    return;
  }
  // Zero initialize |output| so we're accumulating from zero.
  output->ZeroFrames(frame_count);

  // If we're just remapping we can simply copy the correct input to output.
  if (remapping_) {
    const size_t frames = static_cast<size_t>(frame_count);

    for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
      auto output_channel = output->channel_span(output_ch);
      for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
        float scale = matrix_[output_ch][input_ch];
        if (scale > 0) {
          DCHECK_EQ(scale, 1.0f);
          output_channel.first(frames).copy_from_nonoverlapping(
              input->channel_span(input_ch).first(frames));
          break;
        }
      }
    }
    return;
  }

  for (int output_ch = 0; output_ch < output->channels(); ++output_ch) {
    auto output_channel = output->channel_span(output_ch);
    for (int input_ch = 0; input_ch < input->channels(); ++input_ch) {
      float scale = matrix_[output_ch][input_ch];
      // Scale should always be positive.  Don't bother scaling by zero.
      DCHECK_GE(scale, 0);
      const size_t frames = static_cast<size_t>(frame_count);
      if (scale > 0) {
        vector_math::FMAC(input->channel_span(input_ch).first(frames), scale,
                          output_channel.first(frames));
      }
    }
  }
}

}  // namespace media
