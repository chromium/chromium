// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_
#define MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/sinc_resampler.h"

namespace media {
class AudioBus;

// MultiChannelResampler is a multi channel wrapper for SincResampler; allowing
// high quality sample rate conversion of multiple channels at once.
class MEDIA_EXPORT MultiChannelResampler {
 public:
  // Callback type for providing more data into the resampler.  Expects AudioBus
  // to be completely filled with data upon return; zero padded if not enough
  // frames are available to satisfy the request.  |frame_delay| is the number
  // of output frames already processed and can be used to estimate delay.
  typedef base::RepeatingCallback<void(int frame_delay, AudioBus* audio_bus)>
      ReadCB;

  // Constructs a MultiChannelResampler with the specified |read_cb|, which is
  // used to acquire audio data for resampling.  |io_sample_rate_ratio| is the
  // ratio of input / output sample rates.  |request_frames| is the size in
  // frames of the AudioBus to be filled by |read_cb|.
  MultiChannelResampler(int channels,
                        double io_sample_rate_ratio,
                        size_t request_frames,
                        const ReadCB read_cb);
  virtual ~MultiChannelResampler();

  // Resamples |frames| of data from |read_cb_| into AudioBus.
  void Resample(int frames, AudioBus* audio_bus);

  // Flush all buffered data and reset internal indices.  Not thread safe, do
  // not call while Resample() is in progress.
  void Flush();

  // Update ratio for all SincResamplers.  SetRatio() will cause reconstruction
  // of the kernels used for resampling.  Not thread safe, do not call while
  // Resample() is in progress.
  void SetRatio(double io_sample_rate_ratio);

  // The maximum size in frames that guarantees Resample() will only make a
  // single call to |read_cb_| for more data.
  int ChunkSize() const;

  // See SincResampler::BufferedFrames.
  double BufferedFrames() const;

  // See SincResampler::PrimeWithSilence.
  void PrimeWithSilence();

 private:
  // SincResampler::ReadCB implementation.  ProvideInput() will be called for
  // each channel (in channel order) as SincResampler needs more data.
  void ProvideInput(int channel, int frames, float* destination);

  // Source of data for resampling.
  ReadCB read_cb_;

  // Each channel has its own high quality resampler.
  std::vector<std::unique_ptr<SincResampler>> resamplers_;

  // Buffers for audio data going into SincResampler from ReadCB.
  std::unique_ptr<AudioBus> resampler_audio_bus_;

  // To avoid a memcpy() on the first channel we create a wrapped AudioBus where
  // the first channel points to the |destination| provided to ProvideInput().
  std::unique_ptr<AudioBus> wrapped_resampler_audio_bus_;

  // The number of output frames that have successfully been processed during
  // the current Resample() call.
  int output_frames_ready_;

  DISALLOW_COPY_AND_ASSIGN(MultiChannelResampler);
};

}  // namespace media

#endif  // MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_
