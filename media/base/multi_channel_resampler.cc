// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/multi_channel_resampler.h"

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "media/base/audio_bus.h"

namespace media {

MultiChannelResampler::MultiChannelResampler(int channels,
                                             double io_sample_rate_ratio,
                                             size_t request_size,
                                             const ReadCB read_cb)
    : read_cb_(std::move(read_cb)),
      wrapped_resampler_audio_bus_(AudioBus::CreateWrapper(channels)),
      output_frames_ready_(0) {
  // Allocate each channel's resampler.
  resamplers_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    resamplers_.push_back(std::make_unique<SincResampler>(
        io_sample_rate_ratio, request_size,
        base::BindRepeating(&MultiChannelResampler::ProvideInput,
                            base::Unretained(this), i)));
  }

  // Setup the wrapped AudioBus for channel data.
  wrapped_resampler_audio_bus_->set_frames(request_size);

  // Allocate storage for all channels except the first, which will use the
  // |destination| provided to ProvideInput() directly.
  if (channels > 1) {
    resampler_audio_bus_ = AudioBus::Create(channels - 1, request_size);
    for (int i = 0; i < resampler_audio_bus_->channels(); ++i) {
      wrapped_resampler_audio_bus_->SetChannelData(
          i + 1, resampler_audio_bus_->channel(i));
    }
  }
}

MultiChannelResampler::~MultiChannelResampler() = default;

void MultiChannelResampler::Resample(int frames, AudioBus* audio_bus) {
  DCHECK_EQ(static_cast<size_t>(audio_bus->channels()), resamplers_.size());

  // Optimize the single channel case to avoid the chunking process below.
  if (audio_bus->channels() == 1) {
    resamplers_[0]->Resample(frames, audio_bus->channel(0));
    return;
  }

  // We need to ensure that SincResampler only calls ProvideInput once for each
  // channel.  To ensure this, we chunk the number of requested frames into
  // SincResampler::ChunkSize() sized chunks.  SincResampler guarantees it will
  // only call ProvideInput() once when we resample this way.
  output_frames_ready_ = 0;
  while (output_frames_ready_ < frames) {
    int chunk_size = resamplers_[0]->ChunkSize();
    int frames_this_time = std::min(frames - output_frames_ready_, chunk_size);

    // Resample each channel.
    for (size_t i = 0; i < resamplers_.size(); ++i) {
      DCHECK_EQ(chunk_size, resamplers_[i]->ChunkSize());

      // Depending on the sample-rate scale factor, and the internal buffering
      // used in a SincResampler kernel, this call to Resample() will only
      // sometimes call ProvideInput().  However, if it calls ProvideInput() for
      // the first channel, then it will call it for the remaining channels,
      // since they all buffer in the same way and are processing the same
      // number of frames.
      resamplers_[i]->Resample(
          frames_this_time, audio_bus->channel(i) + output_frames_ready_);
    }

    output_frames_ready_ += frames_this_time;
  }
}

void MultiChannelResampler::ProvideInput(int channel,
                                         int frames,
                                         float* destination) {
  // Get the data from the multi-channel provider when the first channel asks
  // for it.  For subsequent channels, we can just dish out the channel data
  // from that (stored in |resampler_audio_bus_|).
  if (channel == 0) {
    wrapped_resampler_audio_bus_->SetChannelData(0, destination);
    read_cb_.Run(output_frames_ready_, wrapped_resampler_audio_bus_.get());
  } else {
    // All channels must ask for the same amount.  This should always be the
    // case, but let's just make sure.
    DCHECK_EQ(frames, wrapped_resampler_audio_bus_->frames());

    // Copy the channel data from what we received from |read_cb_|.
    memcpy(destination, wrapped_resampler_audio_bus_->channel(channel),
           sizeof(*wrapped_resampler_audio_bus_->channel(channel)) * frames);
  }
}

void MultiChannelResampler::Flush() {
  for (size_t i = 0; i < resamplers_.size(); ++i)
    resamplers_[i]->Flush();
}

void MultiChannelResampler::SetRatio(double io_sample_rate_ratio) {
  for (size_t i = 0; i < resamplers_.size(); ++i)
    resamplers_[i]->SetRatio(io_sample_rate_ratio);
}

int MultiChannelResampler::ChunkSize() const {
  DCHECK(!resamplers_.empty());
  return resamplers_[0]->ChunkSize();
}

int MultiChannelResampler::GetMaxInputFramesRequested(
    int output_frames_requested) const {
  DCHECK(!resamplers_.empty());
  return resamplers_[0]->GetMaxInputFramesRequested(output_frames_requested);
}

double MultiChannelResampler::BufferedFrames() const {
  DCHECK(!resamplers_.empty());
  return resamplers_[0]->BufferedFrames();
}

void MultiChannelResampler::PrimeWithSilence() {
  DCHECK(!resamplers_.empty());
  for (size_t i = 0; i < resamplers_.size(); ++i)
    resamplers_[i]->PrimeWithSilence();
}

int MultiChannelResampler::KernelSize() const {
  DCHECK(!resamplers_.empty());
  return resamplers_[0]->KernelSize();
}

}  // namespace media
