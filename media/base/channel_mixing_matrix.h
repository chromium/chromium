// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_MIXING_MATRIX_H_
#define MEDIA_BASE_CHANNEL_MIXING_MATRIX_H_

#include <vector>

#include "base/macros.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT ChannelMixingMatrix {
 public:
  ChannelMixingMatrix(ChannelLayout input_layout,
                      int input_channels,
                      ChannelLayout output_layout,
                      int output_channels);

  ~ChannelMixingMatrix();

  // Create the transformation matrix of input channels to output channels.
  // Updates the empty matrix with the transformation, and returns true
  // if the transformation is just a remapping of channels (no mixing).
  bool CreateTransformationMatrix(std::vector<std::vector<float>>* matrix);

 private:
  // Result transformation of input channels to output channels
  std::vector<std::vector<float>>* matrix_;

  // Input and output channel layout provided during construction.
  ChannelLayout input_layout_;
  int input_channels_;
  ChannelLayout output_layout_;
  int output_channels_;

  // Helper variable for tracking which inputs are currently unaccounted,
  // should be empty after construction completes.
  std::vector<Channels> unaccounted_inputs_;

  // Helper methods for managing unaccounted input channels.
  void AccountFor(Channels ch);
  bool IsUnaccounted(Channels ch) const;

  // Helper methods for checking if |ch| exists in either |input_layout_| or
  // |output_layout_| respectively.
  bool HasInputChannel(Channels ch) const;
  bool HasOutputChannel(Channels ch) const;

  // Helper methods for updating |matrix_| with the proper value for
  // mixing |input_ch| into |output_ch|.  MixWithoutAccounting() does not
  // remove the channel from |unaccounted_inputs_|.
  void Mix(Channels input_ch, Channels output_ch, float scale);
  void MixWithoutAccounting(Channels input_ch, Channels output_ch, float scale);

  DISALLOW_COPY_AND_ASSIGN(ChannelMixingMatrix);
};

}  // namespace media

#endif  // MEDIA_BASE_CHANNEL_MIXING_MATRIX_H_
