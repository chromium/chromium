// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/channel_mixing_matrix.h"

#include <stddef.h>

#include <algorithm>

#include "base/check_op.h"
#include "media/base/channel_mixer.h"

namespace media {

static void ValidateLayout(const ChannelLayoutConfig& config) {
  CHECK_NE(config.channel_layout(), CHANNEL_LAYOUT_NONE);
  CHECK_LE(config.channel_layout(), CHANNEL_LAYOUT_MAX);
  CHECK_NE(config.channel_layout(), CHANNEL_LAYOUT_UNSUPPORTED);
  CHECK_NE(config.channel_layout(), CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);

  // If we have more than one channel, verify a symmetric layout for sanity.
  // The unit test will verify all possible layouts, so this can be a DCHECK.
  // Symmetry allows simplifying the matrix building code by allowing us to
  // assume that if one channel of a pair exists, the other will too.
  if (config.channels() > 1) {
    // Assert that LEFT exists if and only if RIGHT exists, and so on.
    DCHECK_EQ(ChannelOrder(config.channel_layout(), LEFT) >= 0,
              ChannelOrder(config.channel_layout(), RIGHT) >= 0);
    DCHECK_EQ(ChannelOrder(config.channel_layout(), SIDE_LEFT) >= 0,
              ChannelOrder(config.channel_layout(), SIDE_RIGHT) >= 0);
    DCHECK_EQ(ChannelOrder(config.channel_layout(), BACK_LEFT) >= 0,
              ChannelOrder(config.channel_layout(), BACK_RIGHT) >= 0);
    DCHECK_EQ(ChannelOrder(config.channel_layout(), LEFT_OF_CENTER) >= 0,
              ChannelOrder(config.channel_layout(), RIGHT_OF_CENTER) >= 0);
    DCHECK_EQ(ChannelOrder(config.channel_layout(), TOP_FRONT_LEFT) >= 0,
              ChannelOrder(config.channel_layout(), TOP_FRONT_RIGHT) >= 0);
    DCHECK_EQ(ChannelOrder(config.channel_layout(), TOP_BACK_LEFT) >= 0,
              ChannelOrder(config.channel_layout(), TOP_BACK_RIGHT) >= 0);
  } else {
    DCHECK(config.channel_layout() == CHANNEL_LAYOUT_MONO ||
           config.channel_layout() == CHANNEL_LAYOUT_DISCRETE);
  }
}

ChannelMixingMatrix::ChannelMixingMatrix(ChannelLayoutConfig input_config,
                                         ChannelLayoutConfig output_config)
    : input_config_(input_config), output_config_(output_config) {
  // Stereo down mix should never be the output layout.
  CHECK_NE(output_config.channel_layout(), CHANNEL_LAYOUT_STEREO_DOWNMIX);

  // Verify that the layouts are supported
  ValidateLayout(input_config);
  ValidateLayout(output_config);

  // Special case for 5.0, 5.1 with back channels when upmixed to 7.0, 7.1,
  // which should map the back LR to side LR.
  if (input_config_.channel_layout() == CHANNEL_LAYOUT_5_0_BACK &&
      output_config_.channel_layout() == CHANNEL_LAYOUT_7_0) {
    input_config_ = ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_5_0);
  } else if (input_config_.channel_layout() == CHANNEL_LAYOUT_5_1_BACK &&
             output_config_.channel_layout() == CHANNEL_LAYOUT_7_1) {
    input_config_ = ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_5_1);
  }
}

ChannelMixingMatrix::~ChannelMixingMatrix() = default;

bool ChannelMixingMatrix::CreateTransformationMatrix(
    std::vector<std::vector<float>>* matrix) {
  matrix_ = matrix;

  // Size out the initial matrix.
  matrix_->reserve(output_config_.channels());
  for (int output_ch = 0; output_ch < output_config_.channels(); ++output_ch) {
    matrix_->push_back(std::vector<float>(input_config_.channels(), 0));
  }

  // First check for discrete case.
  if (input_config_.channel_layout() == CHANNEL_LAYOUT_DISCRETE ||
      output_config_.channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
    // If the number of input channels is more than output channels, then
    // copy as many as we can then drop the remaining input channels.
    // If the number of input channels is less than output channels, then
    // copy them all, then zero out the remaining output channels.
    int passthrough_channels =
        std::min(input_config_.channels(), output_config_.channels());
    for (int i = 0; i < passthrough_channels; ++i) {
      (*matrix_)[i][i] = 1;
    }

    return true;
  }

  // Route matching channels and figure out which ones aren't accounted for.
  for (Channels ch = LEFT; ch < CHANNELS_MAX + 1;
       ch = static_cast<Channels>(ch + 1)) {
    int input_ch_index = ChannelOrder(input_config_.channel_layout(), ch);
    if (input_ch_index < 0) {
      continue;
    }

    // If input layout is mono or 1.1, and output layout has L/R channel, we
    // expect up mix center channel into L/R channel no matter if output
    // layout has center channel or not.
    const bool force_upmix_center_into_lr_channel =
        ch == CENTER && IsMonoInputLayout() && HasOutputChannel(LEFT);
    int output_ch_index = ChannelOrder(output_config_.channel_layout(), ch);
    if (output_ch_index < 0 || force_upmix_center_into_lr_channel) {
      unaccounted_inputs_.push_back(ch);
      continue;
    }

    DCHECK_LT(static_cast<size_t>(output_ch_index), matrix_->size());
    DCHECK_LT(static_cast<size_t>(input_ch_index),
              (*matrix_)[output_ch_index].size());
    (*matrix_)[output_ch_index][input_ch_index] = 1;
  }

  // If all input channels are accounted for, there's nothing left to do.
  if (unaccounted_inputs_.empty()) {
    // Since all output channels map directly to inputs we can optimize.
    return true;
  }

  // Mix front LR into center.
  if (IsUnaccounted(LEFT)) {
    // When down mixing to mono or 1.1 from stereo, we need to be careful of
    // full scale stereo mixes.  Scaling by 1 / sqrt(2) here will likely lead to
    // clipping so we use 1 / 2 instead.
    float scale =
        IsMonoOutputLayout() && input_config_ == ChannelLayoutConfig::Stereo()
            ? 0.5
            : ChannelMixer::kHalfPower;
    Mix(LEFT, CENTER, scale);
    Mix(RIGHT, CENTER, scale);
  }

  // Mix center into front LR.
  if (IsUnaccounted(CENTER)) {
    // When up mixing from mono or 1.1, just do a copy to front LR.
    float scale = IsMonoInputLayout() ? 1 : ChannelMixer::kHalfPower;
    MixWithoutAccounting(CENTER, LEFT, scale);
    Mix(CENTER, RIGHT, scale);
  }

  // Mix back LR into: side LR || back center || front LR || front center.
  if (IsUnaccounted(BACK_LEFT)) {
    if (HasOutputChannel(SIDE_LEFT)) {
      // If the input has side LR, mix back LR into side LR, but instead if the
      // input doesn't have side LR (but output does) copy back LR to side LR.
      float scale = HasInputChannel(SIDE_LEFT) ? ChannelMixer::kHalfPower : 1;
      Mix(BACK_LEFT, SIDE_LEFT, scale);
      Mix(BACK_RIGHT, SIDE_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {
      // Mix back LR into back center.
      Mix(BACK_LEFT, BACK_CENTER, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, BACK_CENTER, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(LEFT)) {
      // Mix back LR into front LR.
      Mix(BACK_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix back LR into front center.
      Mix(BACK_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix side LR into: back LR || back center || front LR || front center.
  if (IsUnaccounted(SIDE_LEFT)) {
    if (HasOutputChannel(BACK_LEFT)) {
      // If the input has back LR, mix side LR into back LR, but instead if the
      // input doesn't have back LR (but output does) copy side LR to back LR.
      float scale = HasInputChannel(BACK_LEFT) ? ChannelMixer::kHalfPower : 1;
      Mix(SIDE_LEFT, BACK_LEFT, scale);
      Mix(SIDE_RIGHT, BACK_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {
      // Mix side LR into back center.
      Mix(SIDE_LEFT, BACK_CENTER, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, BACK_CENTER, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(LEFT)) {
      // Mix side LR into front LR.
      Mix(SIDE_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix side LR into front center.
      Mix(SIDE_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix back center into: back LR || side LR || front LR || front center.
  if (IsUnaccounted(BACK_CENTER)) {
    if (HasOutputChannel(BACK_LEFT)) {
      // Mix back center into back LR.
      MixWithoutAccounting(BACK_CENTER, BACK_LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, BACK_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(SIDE_LEFT)) {
      // Mix back center into side LR.
      MixWithoutAccounting(BACK_CENTER, SIDE_LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, SIDE_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(LEFT)) {
      // Mix back center into front LR.
      // TODO(dalecurtis): Not sure about these values?
      MixWithoutAccounting(BACK_CENTER, LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix back center into front center.
      // TODO(dalecurtis): Not sure about these values?
      Mix(BACK_CENTER, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix LR of center into: front LR || front center.
  if (IsUnaccounted(LEFT_OF_CENTER)) {
    if (HasOutputChannel(LEFT)) {
      // Mix LR of center into front LR.
      Mix(LEFT_OF_CENTER, LEFT, ChannelMixer::kHalfPower);
      Mix(RIGHT_OF_CENTER, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix LR of center into front center.
      Mix(LEFT_OF_CENTER, CENTER, ChannelMixer::kHalfPower);
      Mix(RIGHT_OF_CENTER, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix top front LR into: LR of center || side LR || front LR || front center.
  if (IsUnaccounted(TOP_FRONT_LEFT)) {
    if (HasOutputChannel(LEFT_OF_CENTER)) {
      // Mix top front LR into LR of center.
      Mix(TOP_FRONT_LEFT, LEFT_OF_CENTER, ChannelMixer::kHalfPower);
      Mix(TOP_FRONT_RIGHT, RIGHT_OF_CENTER, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(SIDE_LEFT)) {
      // Mix top front LR into side LR.
      Mix(TOP_FRONT_LEFT, SIDE_LEFT, ChannelMixer::kHalfPower);
      Mix(TOP_FRONT_RIGHT, SIDE_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(LEFT)) {
      // Mix top front LR into front LR.
      Mix(TOP_FRONT_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(TOP_FRONT_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix top front LR into front center.
      Mix(TOP_FRONT_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(TOP_FRONT_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix top back LR into: back LR || side LR || front LR || front center.
  if (IsUnaccounted(TOP_BACK_LEFT)) {
    if (HasOutputChannel(BACK_LEFT)) {
      // Mix top back LR into back LR.
      Mix(TOP_BACK_LEFT, BACK_LEFT, ChannelMixer::kHalfPower);
      Mix(TOP_BACK_RIGHT, BACK_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(SIDE_LEFT)) {
      // Mix top back LR into side LR.
      Mix(TOP_BACK_LEFT, SIDE_LEFT, ChannelMixer::kHalfPower);
      Mix(TOP_BACK_RIGHT, SIDE_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(LEFT)) {
      // Mix top back LR into front LR.
      Mix(TOP_BACK_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(TOP_BACK_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix top back LR into front center.
      Mix(TOP_BACK_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(TOP_BACK_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // Mix LFE into: front center || front LR.
  if (IsUnaccounted(LFE)) {
    if (!HasOutputChannel(CENTER)) {
      // Mix LFE into front LR.
      MixWithoutAccounting(LFE, LEFT, ChannelMixer::kHalfPower);
      Mix(LFE, RIGHT, ChannelMixer::kHalfPower);
    } else {
      // Mix LFE into front center.
      Mix(LFE, CENTER, ChannelMixer::kHalfPower);
    }
  }

  // All channels should now be accounted for.
  DCHECK(unaccounted_inputs_.empty());

  // See if the output |matrix_| is simply a remapping matrix.  If each input
  // channel maps to a single output channel we can simply remap.  Doing this
  // programmatically is less fragile than logic checks on channel mappings.
  for (int output_ch = 0; output_ch < output_config_.channels(); ++output_ch) {
    int input_mappings = 0;
    for (int input_ch = 0; input_ch < input_config_.channels(); ++input_ch) {
      // We can only remap if each row contains a single scale of 1.  I.e., each
      // output channel is mapped from a single unscaled input channel.
      if ((*matrix_)[output_ch][input_ch] != 1 || ++input_mappings > 1) {
        return false;
      }
    }
  }

  // If we've gotten here, |matrix_| is simply a remapping.
  return true;
}

void ChannelMixingMatrix::AccountFor(Channels ch) {
  unaccounted_inputs_.erase(std::ranges::find(unaccounted_inputs_, ch));
}

bool ChannelMixingMatrix::IsUnaccounted(Channels ch) const {
  return std::ranges::contains(unaccounted_inputs_, ch);
}

bool ChannelMixingMatrix::IsMonoInputLayout() const {
  return input_config_ == ChannelLayoutConfig::Mono() ||
         input_config_ == ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>();
}

bool ChannelMixingMatrix::IsMonoOutputLayout() const {
  return output_config_ == ChannelLayoutConfig::Mono() ||
         output_config_ ==
             ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_1_1>();
}

bool ChannelMixingMatrix::HasInputChannel(Channels ch) const {
  return ChannelOrder(input_config_.channel_layout(), ch) >= 0;
}

bool ChannelMixingMatrix::HasOutputChannel(Channels ch) const {
  return ChannelOrder(output_config_.channel_layout(), ch) >= 0;
}

void ChannelMixingMatrix::Mix(Channels input_ch,
                              Channels output_ch,
                              float scale) {
  MixWithoutAccounting(input_ch, output_ch, scale);
  AccountFor(input_ch);
}

void ChannelMixingMatrix::MixWithoutAccounting(Channels input_ch,
                                               Channels output_ch,
                                               float scale) {
  int input_ch_index = ChannelOrder(input_config_.channel_layout(), input_ch);
  int output_ch_index =
      ChannelOrder(output_config_.channel_layout(), output_ch);

  DCHECK(IsUnaccounted(input_ch));
  DCHECK_GE(input_ch_index, 0);
  DCHECK_GE(output_ch_index, 0);

  DCHECK_EQ((*matrix_)[output_ch_index][input_ch_index], 0);
  (*matrix_)[output_ch_index][input_ch_index] = scale;
}

}  // namespace media
