// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/audio/mixing_graph_impl.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/loopback_audio_converter.h"
#include "services/audio/sync_mixing_graph_input.h"

namespace audio {
namespace {
std::unique_ptr<media::LoopbackAudioConverter> CreateConverter(
    const media::AudioParameters& input_params,
    const media::AudioParameters& output_params) {
  return std::make_unique<media::LoopbackAudioConverter>(
      input_params, output_params, /*disable_fifo=*/true);
}

// Clamps all samples to the interval [-1, 1].
void SanitizeOutput(media::AudioBus* bus) {
  for (int channel = 0; channel < bus->channels(); ++channel) {
    float* data = bus->channel(channel);
    for (int frame = 0; frame < bus->frames(); frame++) {
      float value = data[frame];
      if (value * value <= 1.0f) [[likely]] {
        continue;
      }
      // The sample is out of range. Negative values are clamped to -1. Positive
      // values and NaN are clamped to 1.
      data[frame] = value < 0.0f ? -1.0f : 1.0f;
    }
  }
}

bool SameChannelSetup(const media::AudioParameters& a,
                      const media::AudioParameters& b) {
  return a.channel_layout() == b.channel_layout() &&
         a.channels() == b.channels();
}
}  // namespace

// Counts how often mixing callback duration exceeded the given time limit and
// logs it as a UMA histogram.
class MixingGraphImpl::OvertimeLogger {
 public:
  // Logs once every 10s, assuming 10ms buffers.
  constexpr static int kCallbacksPerLogPeriod = 1000;

  explicit OvertimeLogger(base::TimeDelta timeout) : timeout_(timeout) {}

  void Log(base::TimeTicks callback_start) {
    ++callback_count_;

    if (base::TimeTicks::Now() - callback_start > timeout_)
      overtime_count_++;

    if (callback_count_ % kCallbacksPerLogPeriod)
      return;

    // Clipped to 100 to give more resolution to lower values.
    base::UmaHistogramCounts100(
        "Media.Audio.OutputDeviceMixer.OvertimeCount", overtime_count_);

    overtime_count_ = 0;
  }

 private:
  const base::TimeDelta timeout_;
  int callback_count_ = 0;
  int overtime_count_ = 0;
};

MixingGraphImpl::MixingGraphImpl(const media::AudioParameters& output_params,
                                 OnMoreDataCallback on_more_data_cb,
                                 OnErrorCallback on_error_cb)
    : MixingGraphImpl(output_params,
                      on_more_data_cb,
                      on_error_cb,
                      base::BindRepeating(&CreateConverter)) {}

MixingGraphImpl::MixingGraphImpl(const media::AudioParameters& output_params,
                                 OnMoreDataCallback on_more_data_cb,
                                 OnErrorCallback on_error_cb,
                                 CreateConverterCallback create_converter_cb)
    : output_params_(output_params),
      on_more_data_cb_(std::move(on_more_data_cb)),
      on_error_cb_(std::move(on_error_cb)),
      create_converter_cb_(std::move(create_converter_cb)),
      overtime_logger_(
          std::make_unique<OvertimeLogger>(output_params.GetBufferDuration())),
      main_converter_(output_params, output_params, /*disable_fifo=*/true) {}

MixingGraphImpl::~MixingGraphImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(main_converter_.empty());
  DCHECK(converters_.empty());
}

std::unique_ptr<MixingGraph::Input> MixingGraphImpl::CreateInput(
    const media::AudioParameters& params) {
  return std::make_unique<SyncMixingGraphInput>(this, params);
}

media::LoopbackAudioConverter* MixingGraphImpl::FindOrAddConverter(
    const media::AudioParameters& input_params,
    const media::AudioParameters& output_params,
    media::LoopbackAudioConverter* parent_converter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  AudioConverterKey key(input_params);
  auto converter = converters_.find(key);
  if (converter == converters_.end()) {
    // No existing suitable converter. Add a new converter to the graph.
    std::pair<AudioConverters::iterator, bool> result =
        converters_.insert(std::make_pair(
            key, create_converter_cb_.Run(input_params, output_params)));
    converter = result.first;

    // Add the new converter as an input to its parent converter.
    base::AutoLock scoped_lock(lock_);
    if (parent_converter) {
      parent_converter->AddInput(converter->second.get());
    } else {
      main_converter_.AddInput(converter->second.get());
    }
  }

  return converter->second.get();
}

void MixingGraphImpl::AddInput(Input* input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  const auto& input_params = input->GetParams();
  DCHECK(input_params.format() ==
         media::AudioParameters::AUDIO_PCM_LOW_LATENCY);

  // Resampler input format is the same as output except sample rate.
  media::AudioParameters resampler_input_params(output_params_);
  resampler_input_params.set_sample_rate(input_params.sample_rate());

  // Channel mixer input format is the same as resampler input except channel
  // layout and channel count.
  media::AudioParameters channel_mixer_input_params(
      resampler_input_params.format(), input_params.channel_layout_config(),
      resampler_input_params.sample_rate(),
      resampler_input_params.frames_per_buffer());

  media::LoopbackAudioConverter* converter = nullptr;

  // Check if resampling is needed.
  if (resampler_input_params.sample_rate() != output_params_.sample_rate()) {
    // Re-use or create a resampler.
    converter =
        FindOrAddConverter(resampler_input_params, output_params_, converter);
  }

  // Check if channel mixing is needed.
  if (!SameChannelSetup(channel_mixer_input_params, resampler_input_params)) {
    // Re-use or create a channel mixer.
    converter = FindOrAddConverter(channel_mixer_input_params,
                                   resampler_input_params, converter);
  }

  // Add the input to the mixing graph.
  base::AutoLock scoped_lock(lock_);
  if (converter) {
    converter->AddInput(input);
  } else {
    main_converter_.AddInput(input);
  }
}

void MixingGraphImpl::Remove(const AudioConverterKey& key,
                             media::AudioConverter::InputCallback* input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (key == AudioConverterKey(output_params_)) {
    base::AutoLock scoped_lock(lock_);
    main_converter_.RemoveInput(input);
    return;
  }

  auto converter = converters_.find(key);
  CHECK(converter != converters_.end(), base::NotFatalUntil::M130);
  media::LoopbackAudioConverter* parent = converter->second.get();
  {
    base::AutoLock scoped_lock(lock_);
    parent->RemoveInput(input);
  }

  // Remove parent converter if empty.
  if (parent->empty()) {
    // With knowledge of the tree structure (resampling closer to the
    // main converter than channel mixing) the key of the grandparent converter
    // can be deduced. This key is used to find the grandparent and remove the
    // reference to the empty parent converter.
    AudioConverterKey next_key(key);
    if (!key.SameChannelSetup(output_params_)) {
      next_key.UpdateChannelSetup(output_params_);
    } else {
      // If the parent converter is not the main converter its key (and input
      // parameters) should differ from the output parameters in sample rate,
      // channel setup or both.
      DCHECK_NE(key.sample_rate(), output_params_.sample_rate());
      next_key.set_sample_rate(output_params_.sample_rate());
    }
    Remove(next_key, parent);
    converters_.erase(converter);
  }
}

void MixingGraphImpl::RemoveInput(Input* input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  Remove(AudioConverterKey(input->GetParams()), input);
}

int MixingGraphImpl::OnMoreData(base::TimeDelta delay,
                                base::TimeTicks delay_timestamp,
                                const media::AudioGlitchInfo& glitch_info,
                                media::AudioBus* dest) {
  const base::TimeTicks start_time(base::TimeTicks::Now());

  uint32_t frames_delayed = media::AudioTimestampHelper::TimeToFrames(
      delay, output_params_.sample_rate());

  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("audio"), "MixingGraphImpl::OnMoreData",
              "playout_delay (ms)", delay.InMillisecondsF(),
              "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
              "delay (frames)", frames_delayed);
  {
    base::AutoLock scoped_lock(lock_);
    main_converter_.ConvertWithInfo(frames_delayed, glitch_info, dest);
  }

  SanitizeOutput(dest);

  on_more_data_cb_.Run(*dest, delay);

  overtime_logger_->Log(start_time);
  return dest->frames();
}

void MixingGraphImpl::OnError(ErrorType error) {
  on_error_cb_.Run(error);
}
}  // namespace audio
