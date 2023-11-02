// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A set of utility functions to perform WSOLA.

#ifndef MEDIA_FILTERS_WSOLA_INTERNALS_H_
#define MEDIA_FILTERS_WSOLA_INTERNALS_H_

#include <utility>

#include "media/base/media_export.h"

namespace media {

class AudioBus;

namespace internal {

typedef std::pair<int, int> Interval;

// Dot-product of channels of two AudioBus. For each AudioBus an offset is
// given. |dot_product[k]| is the dot-product of channel |k|. The caller should
// allocate sufficient space for |dot_product|.
MEDIA_EXPORT void MultiChannelDotProduct(const AudioBus* a,
                                         int frame_offset_a,
                                         const AudioBus* b,
                                         int frame_offset_b,
                                         int num_frames,
                                         float* dot_product);

// Energies of sliding windows of channels are interleaved.
// The number windows is |input->frames()| - (|frames_per_window| - 1), hence,
// the method assumes |energy| must be, at least, of size
// (|input->frames()| - (|frames_per_window| - 1)) * |input->channels()|.
MEDIA_EXPORT void MultiChannelMovingBlockEnergies(const AudioBus* input,
                                                  int frames_per_window,
                                                  float* energy);

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the maximum, assuming that y[0] <= y[1] >= y[2].
MEDIA_EXPORT void QuadraticInterpolation(const float* y_values,
                                         float* extremum,
                                         float* extremum_value);

// Search a subset of all candid blocks. The search is performed every
// |decimation| frames. This reduces complexity by a factor of about
// 1 / |decimation|. A cubic interpolation is used to have a better estimate of
// the best match.
MEDIA_EXPORT int DecimatedSearch(int decimation,
                                 Interval exclude_interval,
                                 const AudioBus* target_block,
                                 const AudioBus* search_segment,
                                 const float* energy_target_block,
                                 const float* energy_candid_blocks);

// Search [|low_limit|, |high_limit|] of |search_segment| to find a block that
// is most similar to |target_block|. |energy_target_block| is the energy of the
// |target_block|. |energy_candidate_blocks| is the energy of all blocks within
// |search_block|.
MEDIA_EXPORT int FullSearch(int low_limit,
                            int hight_limimit,
                            Interval exclude_interval,
                            const AudioBus* target_block,
                            const AudioBus* search_block,
                            const float* energy_target_block,
                            const float* energy_candidate_blocks);

// Find the index of the block, within |search_block|, that is most similar
// to |target_block|. Obviously, the returned index is w.r.t. |search_block|.
// |exclude_interval| is an interval that is excluded from the search.
MEDIA_EXPORT int OptimalIndex(const AudioBus* search_block,
                              const AudioBus* target_block,
                              Interval exclude_interval);

// Return a "periodic" Hann window. This is the first L samples of an L+1
// Hann window. It is perfect reconstruction for overlap-and-add.
MEDIA_EXPORT void GetPeriodicHanningWindow(int window_length, float* window);

}  // namespace internal

}  // namespace media

#endif  // MEDIA_FILTERS_WSOLA_INTERNALS_H_
