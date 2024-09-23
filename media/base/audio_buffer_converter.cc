// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/audio_buffer_converter.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/check_op.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/sinc_resampler.h"
#include "media/base/timestamp_constants.h"
#include "media/base/vector_math.h"

namespace media {

// Is the config presented by |buffer| a config change from |params|?
static bool IsConfigChange(const AudioParameters& params,
                           const AudioBuffer& buffer) {
  return buffer.sample_rate() != params.sample_rate() ||
         buffer.channel_count() != params.channels() ||
         buffer.channel_layout() != params.channel_layout();
}

AudioBufferConverter::AudioBufferConverter(const AudioParameters& output_params)
    : output_params_(output_params),
      input_params_(output_params),
      last_input_buffer_offset_(0),
      input_frames_(0),
      buffered_input_frames_(0.0),
      io_sample_rate_ratio_(1.0),
      timestamp_helper_(output_params_.sample_rate()),
      is_flushing_(false),
      pool_(new AudioBufferMemoryPool()) {}

AudioBufferConverter::~AudioBufferConverter() = default;

void AudioBufferConverter::AddInput(scoped_refptr<AudioBuffer> buffer) {
  // On EOS flush any remaining buffered data.
  if (buffer->end_of_stream()) {
    Flush();
    queued_outputs_.push_back(std::move(buffer));
    return;
  }

  // We'll need a new |audio_converter_| if there was a config change.
  if (IsConfigChange(input_params_, *buffer))
    ResetConverter(*buffer);

  // Pass straight through if there's no work to be done.
  if (!audio_converter_) {
    queued_outputs_.push_back(std::move(buffer));
    return;
  }

  if (!timestamp_helper_.base_timestamp()) {
    timestamp_helper_.SetBaseTimestamp(buffer->timestamp());
  }

  input_frames_ += buffer->frame_count();
  queued_inputs_.push_back(std::move(buffer));

  ConvertIfPossible();
}

bool AudioBufferConverter::HasNextBuffer() { return !queued_outputs_.empty(); }

scoped_refptr<AudioBuffer> AudioBufferConverter::GetNextBuffer() {
  DCHECK(!queued_outputs_.empty());
  auto out = std::move(queued_outputs_.front());
  queued_outputs_.pop_front();
  return out;
}

void AudioBufferConverter::Reset() {
  audio_converter_.reset();
  queued_inputs_.clear();
  queued_outputs_.clear();
  timestamp_helper_.Reset();
  input_params_ = output_params_;
  input_frames_ = 0;
  buffered_input_frames_ = 0.0;
  last_input_buffer_offset_ = 0;
}

void AudioBufferConverter::ResetTimestampState() {
  Flush();
  timestamp_helper_.Reset();
}

double AudioBufferConverter::ProvideInput(AudioBus* audio_bus,
                                          uint32_t frames_delayed,
                                          const AudioGlitchInfo& glitch_info) {
  DCHECK(is_flushing_ || input_frames_ >= audio_bus->frames());

  int requested_frames_left = audio_bus->frames();
  int dest_index = 0;

  while (requested_frames_left > 0 && !queued_inputs_.empty()) {
    const auto& input_buffer = queued_inputs_.front();

    int frames_to_read =
        std::min(requested_frames_left,
                 input_buffer->frame_count() - last_input_buffer_offset_);
    input_buffer->ReadFrames(frames_to_read, last_input_buffer_offset_,
                             dest_index, audio_bus);
    last_input_buffer_offset_ += frames_to_read;

    if (last_input_buffer_offset_ == input_buffer->frame_count()) {
      // We've consumed all the frames in |input_buffer|.
      queued_inputs_.pop_front();
      last_input_buffer_offset_ = 0;
    }

    requested_frames_left -= frames_to_read;
    dest_index += frames_to_read;
  }

  // If we're flushing, zero any extra space, otherwise we should always have
  // enough data to completely fulfill the request.
  if (is_flushing_ && requested_frames_left > 0) {
    audio_bus->ZeroFramesPartial(audio_bus->frames() - requested_frames_left,
                                 requested_frames_left);
  } else {
    DCHECK_EQ(requested_frames_left, 0);
  }

  input_frames_ -= audio_bus->frames() - requested_frames_left;
  DCHECK_GE(input_frames_, 0);

  buffered_input_frames_ += audio_bus->frames() - requested_frames_left;

  // Full volume.
  return 1.0;
}

void AudioBufferConverter::ResetConverter(const AudioBuffer& buffer) {
  Flush();
  audio_converter_.reset();
  input_params_.Reset(
      input_params_.format(), {buffer.channel_layout(), buffer.channel_count()},
      buffer.sample_rate(),
      // If resampling is needed and the FIFO disabled, the AudioConverter will
      // always request SincResampler::kDefaultRequestSize frames.  Otherwise it
      // will use the output frame size.
      buffer.sample_rate() == output_params_.sample_rate()
          ? output_params_.frames_per_buffer()
          : SincResampler::kDefaultRequestSize);

  io_sample_rate_ratio_ = static_cast<double>(input_params_.sample_rate()) /
                          output_params_.sample_rate();

  // If |buffer| matches |output_params_| we don't need an AudioConverter at
  // all, and can early-out here.
  if (!IsConfigChange(output_params_, buffer))
    return;

  // Note: The FIFO is disabled to avoid extraneous memcpy().
  audio_converter_ =
      std::make_unique<AudioConverter>(input_params_, output_params_, true);
  audio_converter_->AddInput(this);
}

void AudioBufferConverter::ConvertIfPossible() {
  DCHECK(audio_converter_);

  int request_frames = 0;

  if (is_flushing_) {
    // If we're flushing we want to convert *everything* even if this means
    // we'll have to pad some silence in ProvideInput().
    request_frames =
        ceil((buffered_input_frames_ + input_frames_) / io_sample_rate_ratio_);
  } else {
    // How many calls to ProvideInput() we can satisfy completely.
    int chunks = input_frames_ / input_params_.frames_per_buffer();

    // How many output frames that corresponds to:
    request_frames = chunks * audio_converter_->ChunkSize();
  }

  if (!request_frames)
    return;

  auto output_buffer = AudioBuffer::CreateBuffer(
      kSampleFormatPlanarF32, output_params_.channel_layout(),
      output_params_.channels(), output_params_.sample_rate(), request_frames,
      pool_);
  std::unique_ptr<AudioBus> output_bus =
      AudioBus::CreateWrapper(output_buffer->channel_count());

  int frames_remaining = request_frames;

  // The AudioConverter wants requests of a fixed size, so we'll slide an
  // AudioBus of that size across the |output_buffer|.
  while (frames_remaining != 0) {
    // It's important that this is a multiple of AudioBus::kChannelAlignment in
    // all requests except for the last, otherwise downstream SIMD optimizations
    // will crash on unaligned data.
    const int frames_this_iteration = std::min(
        static_cast<int>(SincResampler::kDefaultRequestSize), frames_remaining);
    const int offset_into_buffer =
        output_buffer->frame_count() - frames_remaining;

    // Wrap the portion of the AudioBuffer in an AudioBus so the AudioConverter
    // can fill it.
    output_bus->set_frames(frames_this_iteration);
    for (int ch = 0; ch < output_buffer->channel_count(); ++ch) {
      output_bus->SetChannelData(
          ch,
          reinterpret_cast<float*>(output_buffer->channel_data()[ch]) +
              offset_into_buffer);
    }

    // Do the actual conversion.
    audio_converter_->Convert(output_bus.get());
    frames_remaining -= frames_this_iteration;
    buffered_input_frames_ -= frames_this_iteration * io_sample_rate_ratio_;
  }

  // Compute the timestamp.
  output_buffer->set_timestamp(timestamp_helper_.GetTimestamp());
  timestamp_helper_.AddFrames(request_frames);

  queued_outputs_.push_back(std::move(output_buffer));
}

void AudioBufferConverter::Flush() {
  if (!audio_converter_)
    return;
  is_flushing_ = true;
  ConvertIfPossible();
  is_flushing_ = false;
  audio_converter_->Reset();
  DCHECK_EQ(input_frames_, 0);
  DCHECK_EQ(last_input_buffer_offset_, 0);
  DCHECK_LT(buffered_input_frames_, 1.0);
  DCHECK(queued_inputs_.empty());
  buffered_input_frames_ = 0.0;
}

}  // namespace media
