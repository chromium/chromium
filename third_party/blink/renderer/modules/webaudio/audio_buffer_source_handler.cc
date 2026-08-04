// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_handler.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_buffer_source_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

namespace {

constexpr double kDefaultGrainDuration = 0.020;  // 20ms

// Arbitrary upper limit on playback rate.
// Higher than expected rates can be useful when playing back oversampled
// buffers to minimize linear interpolation aliasing.
constexpr double kMaxRate = 1024.0;

// Default to mono. A call to setBuffer() will set the number of output
// channels to that of the buffer.
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

AudioBufferSourceHandler::AudioBufferSourceHandler(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& playback_rate,
    AudioParamHandler& detune)
    : AudioScheduledSourceHandler(NodeType::kNodeTypeAudioBufferSource,
                                  node,
                                  sample_rate),
      playback_rate_(&playback_rate),
      detune_(&detune),
      grain_duration_(kDefaultGrainDuration) {
  AddOutput(kDefaultNumberOfOutputChannels);

  Initialize();
}

scoped_refptr<AudioBufferSourceHandler> AudioBufferSourceHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& playback_rate,
    AudioParamHandler& detune) {
  return base::AdoptRef(
      new AudioBufferSourceHandler(node, sample_rate, playback_rate, detune));
}

AudioBufferSourceHandler::~AudioBufferSourceHandler() {
  Uninitialize();
}

void AudioBufferSourceHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "AudioBufferSourceHandler::Process");

  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized()) {
    output_bus->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call TryLock() instead.
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    if (!Buffer()) {
      output_bus->Zero();
      if (GetPlaybackState() != UNSCHEDULED_STATE) {
        Finish();
      }
      return;
    }

    // After calling setBuffer() with a buffer having a different number of
    // channels, there can in rare cases be a slight delay before the output bus
    // is updated to the new number of channels because of use of TryLocks() in
    // the context's updating system.  In this case, if the the buffer has just
    // been changed and we're not quite ready yet, then just output silence.
    if (NumberOfChannels() != shared_buffer_->numberOfChannels()) {
      output_bus->Zero();
      return;
    }

    uint32_t quantum_frame_offset;
    uint32_t buffer_frames_to_process;
    double start_time_offset;

    std::tie(quantum_frame_offset, buffer_frames_to_process,
             start_time_offset) =
        UpdateSchedulingInfo(frames_to_process, output_bus);

    if (!buffer_frames_to_process) {
      output_bus->Zero();
      return;
    }

    for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i) {
      destination_channels_[i] = output_bus->Channel(i)->MutableSpan();
    }

    // Render by reading directly from the buffer.
    if (!RenderFromBuffer(output_bus, quantum_frame_offset,
                          buffer_frames_to_process, start_time_offset)) {
      output_bus->Zero();
      return;
    }

    output_bus->ClearSilentFlag();
  } else {
    // Too bad - the TryLock() failed.  We must be in the middle of changing
    // buffers and were already outputting silence anyway.
    output_bus->Zero();
  }
}

void AudioBufferSourceHandler::RenderSilenceAndFinish(
    size_t index,
    size_t frames_to_process) {
  if (frames_to_process > 0) {
    for (auto& destination : destination_channels_) {
      std::ranges::fill(destination.subspan(index, frames_to_process), 0.0f);
    }
  }
  Finish();
}

AudioBufferSourceHandler::ProcessResult
AudioBufferSourceHandler::ProcessFastPath(double virtual_delta_frames,
                                          double virtual_end_frame,
                                          uint32_t buffer_length,
                                          size_t destination_length,
                                          unsigned number_of_channels,
                                          int frames_to_process,
                                          unsigned write_index,
                                          double virtual_read_index) {
  unsigned read_index = static_cast<unsigned>(virtual_read_index);
  const unsigned delta_frames = static_cast<unsigned>(virtual_delta_frames);
  const unsigned end_frame = static_cast<unsigned>(virtual_end_frame);

  while (frames_to_process > 0) {
    const int frames_to_end = end_frame - read_index;
    int frames_this_time = std::min(frames_to_process, frames_to_end);
    frames_this_time = std::max(0, frames_this_time);
    const size_t frames_this_time_size =
        base::checked_cast<size_t>(frames_this_time);

    DCHECK_LE(write_index + frames_this_time, destination_length);
    DCHECK_LE(read_index + frames_this_time, buffer_length);

    for (unsigned i = 0; i < number_of_channels; ++i) {
      auto dest =
          destination_channels_[i].subspan(write_index, frames_this_time_size);

      if (!source_channels_[i].empty()) {
        dest.copy_from(
            source_channels_[i].subspan(read_index, frames_this_time_size));
      } else {
        std::ranges::fill(dest, 0.0f);
      }
    }

    write_index += frames_this_time;
    read_index += frames_this_time;
    frames_to_process -= frames_this_time;

    // It can happen that `frames_this_time` is 0. DCHECK that we will
    // actually exit the loop in this case.  `frames_this_time` is 0 only if
    // `read_index` >= `end_frame`.
    DCHECK(frames_this_time ? true : read_index >= end_frame);

    // Wrap-around.
    double temp_read_index = read_index;

    if (delta_frames <= 0) {
      Finish();
      break;
    }

    if (temp_read_index >= end_frame) {
      if (!is_looping_) {
        // We're not looping and we've reached the end of the sample data.
        // Generate silence for any remaining samples and stop playing.
        RenderSilenceAndFinish(write_index, frames_to_process);
        read_index = std::min(read_index, buffer_length);
        break;
      }

      // Used to retain the sub-sample fractional frame position across loop
      // bounds.
      const double overflow = temp_read_index - end_frame;
      temp_read_index =
          end_frame - delta_frames + std::fmod(overflow, delta_frames);
    }

    read_index = static_cast<unsigned>(temp_read_index);
  }
  virtual_read_index = read_index;

  return ProcessResult{write_index, virtual_read_index};
}

AudioBufferSourceHandler::ProcessResult
AudioBufferSourceHandler::ProcessInterpolatedPath(double virtual_start_frame,
                                                  double virtual_delta_frames,
                                                  double virtual_end_frame,
                                                  uint32_t buffer_length,
                                                  unsigned number_of_channels,
                                                  double computed_playback_rate,
                                                  int frames_to_process,
                                                  unsigned write_index,
                                                  double virtual_read_index) {
  DCHECK_GT(buffer_length, 0u);
  const unsigned max_index = buffer_length - 1;

  for (int i = 0; i < frames_to_process; ++i) {
    const uint32_t frames_remaining = frames_to_process - i - 1;
    unsigned read_index = static_cast<unsigned>(virtual_read_index);
    const double interpolation_factor = virtual_read_index - read_index;

    // For linear interpolation we need the next sample-frame too.
    unsigned read_index2 = read_index + 1;

    // If we are crossing the loop end boundary, the next sample for
    // interpolation wraps around to the start of the loop. Note: We strictly
    // require `read_index < virtual_end_frame` because negative playback rates
    // can start in the tail past the loop end.
    if (is_looping_ && read_index < virtual_end_frame &&
        read_index2 >= virtual_end_frame) {
      // If we hit the end of the loop, the next sample for interpolation is
      // the start of the loop. We calculate this instead of directly setting to
      // virtual_start_frame to preserve fractional loop bounds, and defensively
      // clamp to protect against floating point rounding issues.
      double wrapped_index;
      if (virtual_delta_frames >= 1.0) {
        wrapped_index = virtual_read_index + 1.0 - virtual_delta_frames;
      } else if (virtual_delta_frames > 0) {
        wrapped_index =
            virtual_start_frame +
            std::fmod(virtual_read_index + 1.0 - virtual_start_frame,
                      virtual_delta_frames);
      } else {
        wrapped_index = virtual_start_frame;
      }
      read_index2 =
          wrapped_index >= 0 ? static_cast<unsigned>(wrapped_index) : 0;
    } else if (read_index2 >= buffer_length) {
      read_index2 = read_index;
    }

    read_index = std::min(read_index, max_index);
    read_index2 = std::min(read_index2, max_index);

    // Linear interpolation.
    for (unsigned channel = 0; channel < number_of_channels; ++channel) {
      auto destination = destination_channels_[channel];
      auto source = source_channels_[channel];

      // The source channel may have been transferred already, so don't try
      // to read from it if it was. Just set the destination to 0.
      if (!source.empty()) {
        double sample;
        if (read_index == read_index2 && read_index >= 1) {
          // We're at the end of the buffer, so just linearly extrapolate
          // from the last two samples.
          const double sample1 = source[read_index - 1];
          const double sample2 = source[read_index];
          sample = sample2 + (sample2 - sample1) * interpolation_factor;
        } else {
          const double sample1 = source[read_index];
          const double sample2 = source[read_index2];
          sample = (1.0 - interpolation_factor) * sample1 +
                   interpolation_factor * sample2;
        }
        destination[write_index] = ClampTo<float>(sample);
      } else {
        destination[write_index] = 0;
      }
    }
    ++write_index;

    virtual_read_index += computed_playback_rate;

    // Wrap-around, retaining sub-sample position since virtualReadIndex is
    // floating-point.
    if (virtual_delta_frames <= 0) {
      Finish();
      break;
    }

    if (computed_playback_rate >= 0 &&
        virtual_read_index >= virtual_end_frame) {
      if (!is_looping_) {
        // We're not looping and we've reached the end of the sample data.
        // Generate silence for any remaining samples and stop playing.
        RenderSilenceAndFinish(write_index, frames_remaining);
        virtual_read_index =
            std::min(virtual_read_index, static_cast<double>(buffer_length));
        break;
      }

      // Used to retain the sub-sample fractional frame position across loop
      // bounds.
      const double overflow = virtual_read_index - virtual_end_frame;
      virtual_read_index = virtual_end_frame - virtual_delta_frames +
                           std::fmod(overflow, virtual_delta_frames);
    } else if (computed_playback_rate < 0 &&
               virtual_read_index < virtual_start_frame) {
      if (!is_looping_) {
        // We're not looping and we've reached the end of the sample data.
        // Generate silence for any remaining samples and stop playing.
        RenderSilenceAndFinish(write_index, frames_remaining);
        virtual_read_index = std::max(0.0, virtual_read_index);
        break;
      }

      // Used to retain the sub-sample fractional frame position across loop
      // bounds.
      const double overflow = virtual_start_frame - virtual_read_index;
      virtual_read_index = virtual_start_frame + virtual_delta_frames -
                           std::fmod(overflow, virtual_delta_frames);
    }
  }

  // Update frames_to_process for the caller.
  frames_to_process = 0;

  return ProcessResult{write_index, virtual_read_index};
}

bool AudioBufferSourceHandler::RenderFromBuffer(
    AudioBus* bus,
    unsigned destination_frame_offset,
    uint32_t number_of_frames,
    double start_time_offset) {
  DCHECK(Context()->IsAudioThread());

  // Basic sanity checking
  DCHECK(bus);
  DCHECK(Buffer());

  unsigned number_of_channels = NumberOfChannels();
  unsigned bus_number_of_channels = bus->NumberOfChannels();

  bool channel_count_good =
      number_of_channels && number_of_channels == bus_number_of_channels;
  DCHECK(channel_count_good);

  // Sanity check destinationFrameOffset, numberOfFrames.
  size_t destination_length = bus->length();

  DCHECK_LE(destination_length, GetDeferredTaskHandler().RenderQuantumFrames());
  DCHECK_LE(number_of_frames, GetDeferredTaskHandler().RenderQuantumFrames());

  DCHECK_LE(destination_frame_offset, destination_length);
  DCHECK_LE(destination_frame_offset + number_of_frames, destination_length);

  // Potentially zero out initial frames leading up to the offset.
  if (destination_frame_offset) {
    for (unsigned i = 0; i < number_of_channels; ++i) {
      std::ranges::fill(
          destination_channels_[i].first(destination_frame_offset), 0.0f);
    }
  }

  // Offset the pointers to the correct offset frame.
  unsigned write_index = destination_frame_offset;

  uint32_t buffer_length = shared_buffer_->length();
  double buffer_sample_rate = shared_buffer_->sampleRate();

  const double virtual_start_frame = effective_loop_start_ * buffer_sample_rate;
  const double virtual_end_frame = effective_loop_end_ * buffer_sample_rate;
  const double virtual_delta_frames = virtual_end_frame - virtual_start_frame;

  double computed_playback_rate = ComputePlaybackRate();
  if (std::abs(computed_playback_rate) > virtual_delta_frames) {
    return false;
  }

  // Get local copy.
  double virtual_read_index = virtual_read_index_;
  int frames_to_process = number_of_frames;

  // Directional playhead setup
  if (computed_playback_rate >= 0) {
    // 1. Forward Loop Clamping
    if (is_looping_ && virtual_read_index >= virtual_end_frame) {
      virtual_read_index = virtual_start_frame;
      virtual_read_index =
          std::min(virtual_read_index, static_cast<double>(buffer_length - 1));
      virtual_read_index_ = virtual_read_index;
    }
    // 2. Forward Start Time Adjustment
    if (start_time_offset < 0 && computed_playback_rate != 0) {
      double skipped_frames =
          std::abs(start_time_offset * computed_playback_rate);
      virtual_read_index += skipped_frames;
      buffer_played_frames_ += skipped_frames;
    }
  } else {
    // 1. Reverse Loop Clamping
    if (is_looping_ && virtual_read_index < virtual_start_frame) {
      virtual_read_index = virtual_start_frame;
      virtual_read_index_ = virtual_read_index;
    }
    // 2. Reverse Start Time Adjustment
    if (start_time_offset < 0) {
      double skipped_frames =
          std::abs(start_time_offset * computed_playback_rate);
      virtual_read_index -= skipped_frames;
      buffer_played_frames_ += skipped_frames;
    }
    // 3. Reverse Out-Of-Bounds Playhead Catch-up
    // Pre-process silence if starting out-of-bounds backwards so we bypass the
    // legacy check.
    while (frames_to_process > 0 && virtual_read_index >= buffer_length) {
      for (unsigned channel = 0; channel < number_of_channels; ++channel) {
        destination_channels_.as_span()[channel][write_index] = 0.0f;
      }
      ++write_index;
      virtual_read_index += computed_playback_rate;
      frames_to_process--;
    }
  }

  bool is_stopping_this_quantum = false;

  if (is_duration_given_) {
    double max_source_frames = grain_duration_ * buffer_sample_rate;
    double source_frames_left = max_source_frames - buffer_played_frames_;
    if (source_frames_left <= 0.0) {
      frames_to_process = 0;
      is_stopping_this_quantum = true;
    } else if (computed_playback_rate != 0.0) {
      double frames_until_limit =
          std::ceil(source_frames_left / std::abs(computed_playback_rate));
      if (frames_until_limit < frames_to_process) {
        frames_to_process = static_cast<int>(frames_until_limit);
        is_stopping_this_quantum = true;
      }
    }
  }

  if (!is_looping_ &&
      ((computed_playback_rate >= 0 && virtual_read_index >= buffer_length) ||
       (computed_playback_rate < 0 && virtual_read_index < 0))) {
    virtual_read_index =
        std::clamp(virtual_read_index, 0.0, static_cast<double>(buffer_length));
    frames_to_process = 0;
    is_stopping_this_quantum = true;
  }

  DCHECK_GE(virtual_read_index, 0);
  DCHECK_GE(virtual_delta_frames, 0);
  DCHECK_GE(virtual_end_frame, 0);

  // Optimize for the very common case of playing back with
  // computedPlaybackRate == 1.  We can avoid the linear interpolation.
  ProcessResult process_result;
  if (computed_playback_rate == 1 &&
      virtual_read_index == floor(virtual_read_index) &&
      virtual_delta_frames == floor(virtual_delta_frames) &&
      virtual_end_frame == floor(virtual_end_frame)) {
    process_result =
        ProcessFastPath(virtual_delta_frames, virtual_end_frame, buffer_length,
                        destination_length, number_of_channels,
                        frames_to_process, write_index, virtual_read_index);
  } else {
    process_result = ProcessInterpolatedPath(
        virtual_start_frame, virtual_delta_frames, virtual_end_frame,
        buffer_length, number_of_channels, computed_playback_rate,
        frames_to_process, write_index, virtual_read_index);
  }
  write_index = process_result.write_index;
  virtual_read_index = process_result.virtual_read_index;

  bus->ClearSilentFlag();

  // Update tracking exactly once per loop block to save per-sample arithmetic.
  if (computed_playback_rate != 0) {
    uint32_t frames_processed = write_index - destination_frame_offset;
    buffer_played_frames_ +=
        frames_processed * std::abs(computed_playback_rate);
  }

  if (is_stopping_this_quantum) {
    auto destination_channels = destination_channels_.as_span();
    for (unsigned i = 0; i < number_of_channels; ++i) {
      std::ranges::fill(
          destination_channels[i].subspan(
              write_index,
              (destination_frame_offset + number_of_frames) - write_index),
          0.0f);
    }
    Finish();
  }

  virtual_read_index_ = virtual_read_index;

  return true;
}

void AudioBufferSourceHandler::SetBuffer(AudioBuffer* buffer,
                                         ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (buffer && buffer_has_been_set_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot set buffer to non-null after it "
                                      "has been already been set to a non-null "
                                      "buffer");
    return;
  }

  // The context must be locked since changing the buffer can re-configure the
  // number of channels that are output.
  DeferredTaskHandler::GraphAutoLocker context_locker(
      Context()->GetDeferredTaskHandler());

  // This synchronizes with process().
  base::AutoLock process_locker(process_lock_);

  if (!buffer) {
    // Clear out the shared buffer.
    shared_buffer_.reset();
  } else {
    buffer_has_been_set_ = true;

    // Do any necesssary re-configuration to the buffer's number of channels.
    unsigned number_of_channels = buffer->numberOfChannels();

    // This should not be possible since AudioBuffers can't be created with too
    // many channels either.
    if (number_of_channels > BaseAudioContext::MaxNumberOfChannels()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          ExceptionMessages::IndexOutsideRange(
              "number of input channels", number_of_channels, 1u,
              ExceptionMessages::kInclusiveBound,
              BaseAudioContext::MaxNumberOfChannels(),
              ExceptionMessages::kInclusiveBound));
      return;
    }

    shared_buffer_ = buffer->CreateSharedAudioBuffer();

    Output(0).SetNumberOfChannels(number_of_channels);

    source_channels_ = base::HeapArray<base::raw_span<const float>>::WithSize(
        number_of_channels);
    destination_channels_ =
        base::HeapArray<base::raw_span<float>>::WithSize(number_of_channels);

    for (unsigned i = 0; i < number_of_channels; ++i) {
      const base::span<const float> channel = shared_buffer_->ChannelSpan(i);
      if (channel.empty()) {
        source_channels_[i] = {};
        continue;
      }

      DCHECK_EQ(channel.size(), shared_buffer_->length());
      source_channels_[i] = channel;
    }

    // If this is a grain (as set by a previous call to start()), validate the
    // grain parameters now since it wasn't validated when start was called
    // (because there was no buffer then).
    if (is_grain_) {
      ClampGrainParameters(shared_buffer_.get());
    }
  }

  UpdateEffectiveLoopPoints();

  virtual_read_index_ = 0;
  buffer_played_frames_ = 0;
}

unsigned AudioBufferSourceHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

void AudioBufferSourceHandler::ClampGrainParameters(
    const SharedAudioBuffer* buffer) {
  DCHECK(buffer);

  // We have a buffer so we can clip the offset and duration to lie within the
  // buffer.
  double buffer_duration = shared_buffer_->duration();

  grain_offset_ = ClampTo(grain_offset_, 0.0, buffer_duration);

  // If the duration was not explicitly given, use the buffer duration to set
  // the grain duration. Otherwise, we want to use the user-specified value, of
  // course.
  if (!is_duration_given_) {
    grain_duration_ = std::numeric_limits<double>::infinity();
  } else {
    // We want to use the user-specified value. The node will stop after
    // playing for grain_duration_ seconds.
    grain_duration_ =
        ClampTo(grain_duration_, 0.0, std::numeric_limits<double>::infinity());
  }

  // We call timeToSampleFrame here since at playbackRate == 1 we don't want to
  // go through linear interpolation at a sub-sample position since it will
  // degrade the quality. When aligned to the sample-frame the playback will be
  // identical to the PCM data stored in the buffer. Since playbackRate == 1 is
  // very common, it's worth considering quality.
  if (playback_rate_->Value() == 1.0 && detune_->Value() == 0.0) {
    virtual_read_index_ = audio_utilities::TimeToSampleFrame(
        grain_offset_, shared_buffer_->sampleRate());
  } else {
    // TimeToSampleFrame rounds to integers; raw multiplication preserves
    // sub-sample accuracy for fractional rates.
    virtual_read_index_ = grain_offset_ * shared_buffer_->sampleRate();
  }
}

void AudioBufferSourceHandler::UpdateEffectiveLoopPoints() {
  if (!Buffer()) {
    effective_loop_start_ = 0;
    effective_loop_end_ = 0;
    return;
  }

  double buffer_duration = shared_buffer_->duration();

  if (!is_looping_) {
    effective_loop_start_ = 0;
    effective_loop_end_ = buffer_duration;
    return;
  }

  // Clamp loopStart to [0, buffer_duration]
  double start = std::clamp(loop_start_, 0.0, buffer_duration);

  // Resolve loopEnd: 0 means buffer duration, otherwise clamp to [0,
  // buffer_duration]
  double end = loop_end_;
  if (end == 0) {
    end = buffer_duration;
  } else {
    end = std::clamp(end, 0.0, buffer_duration);
  }

  if (start < end) {
    effective_loop_start_ = start;
    effective_loop_end_ = end;
  } else {
    // Fallback to entire buffer
    effective_loop_start_ = 0;
    effective_loop_end_ = buffer_duration;
  }
}

base::WeakPtr<AudioScheduledSourceHandler>
AudioBufferSourceHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AudioBufferSourceHandler::Start(double when,
                                     ExceptionState& exception_state) {
  AudioScheduledSourceHandler::Start(when, exception_state);
}

void AudioBufferSourceHandler::Start(double when,
                                     double grain_offset,
                                     ExceptionState& exception_state) {
  StartSource(when, grain_offset, Buffer() ? shared_buffer_->duration() : 0,
              false, exception_state);
}

void AudioBufferSourceHandler::Start(double when,
                                     double grain_offset,
                                     double grain_duration,
                                     ExceptionState& exception_state) {
  StartSource(when, grain_offset, grain_duration, true, exception_state);
}

void AudioBufferSourceHandler::StartSource(double when,
                                           double grain_offset,
                                           double grain_duration,
                                           bool is_duration_given,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  Context()->NotifySourceNodeStart();

  if (GetPlaybackState() != UNSCHEDULED_STATE) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "cannot call start more than once.");
    return;
  }

  if (when < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound("start time", when, 0.0));
    return;
  }

  if (grain_offset < 0) {
    exception_state.ThrowRangeError(ExceptionMessages::IndexExceedsMinimumBound(
        "offset", grain_offset, 0.0));
    return;
  }

  if (grain_duration < 0) {
    exception_state.ThrowRangeError(ExceptionMessages::IndexExceedsMinimumBound(
        "duration", grain_duration, 0.0));
    return;
  }

  // The node is started. Add a reference to keep us alive so that audio
  // will eventually get played even if Javascript should drop all references
  // to this node. The reference will get dropped when the source has finished
  // playing.
  Context()->NotifySourceNodeStartedProcessing(GetNode());

  // This synchronizes with process(). updateSchedulingInfo will read some of
  // the variables being set here.
  base::AutoLock process_locker(process_lock_);

  is_duration_given_ = is_duration_given;
  is_grain_ = true;
  grain_offset_ = grain_offset;
  grain_duration_ = grain_duration;

  // If `when` < `currentTime()`, the source must start now according to the
  // spec.  So just set `start_time_` to `currentTime()` in this case to start
  // the source now.
  start_time_ = std::max(when, Context()->currentTime());

  if (Buffer()) {
    ClampGrainParameters(Buffer());
  }

  SetPlaybackState(SCHEDULED_STATE);
}

void AudioBufferSourceHandler::SetLoop(bool looping) {
  DCHECK(IsMainThread());

  // This synchronizes with `Process()`.
  base::AutoLock process_locker(process_lock_);

  is_looping_ = looping;
  SetDidSetLooping(looping);
  UpdateEffectiveLoopPoints();
}

void AudioBufferSourceHandler::SetLoopStart(double loop_start) {
  DCHECK(IsMainThread());

  // This synchronizes with `Process()`.
  base::AutoLock process_locker(process_lock_);

  loop_start_ = loop_start;
  UpdateEffectiveLoopPoints();
}

void AudioBufferSourceHandler::SetLoopEnd(double loop_end) {
  DCHECK(IsMainThread());

  // This synchronizes with `Process()`.
  base::AutoLock process_locker(process_lock_);

  loop_end_ = loop_end;
  UpdateEffectiveLoopPoints();
}

double AudioBufferSourceHandler::ComputePlaybackRate() {
  // Incorporate buffer's sample-rate versus BaseAudioContext's sample-rate.
  // Normally it's not an issue because buffers are loaded at the
  // BaseAudioContext's sample-rate, but we can handle it in any case.
  double sample_rate_factor = 1.0;
  if (Buffer()) {
    // Use doubles to compute this to full accuracy.
    sample_rate_factor = shared_buffer_->sampleRate() /
                         static_cast<double>(Context()->sampleRate());
  }

  // Use finalValue() to incorporate changes of AudioParamTimeline and
  // AudioSummingJunction from m_playbackRate AudioParam.
  double base_playback_rate = playback_rate_->FinalValue();

  double final_playback_rate = sample_rate_factor * base_playback_rate;

  // Take the detune value into account for the final playback rate.
  final_playback_rate *= fdlibm::pow(2, detune_->FinalValue() / 1200);

  // Sanity check the total rate.  It's very important that the resampler not
  // get any bad rate values.
  final_playback_rate = ClampTo(final_playback_rate, -kMaxRate, kMaxRate);

  DCHECK(!std::isnan(final_playback_rate));
  DCHECK(!std::isinf(final_playback_rate));

  // Record the minimum playback rate for use by HandleStoppableSourceNode.
  if (final_playback_rate < min_playback_rate_) {
    min_playback_rate_ = final_playback_rate;
  }

  return final_playback_rate;
}

double AudioBufferSourceHandler::GetMinPlaybackRate() {
  DCHECK(Context()->IsAudioThread());
  return min_playback_rate_;
}

bool AudioBufferSourceHandler::PropagatesSilence() const {
  DCHECK(Context()->IsAudioThread());

  // A null buffer source still needs to process to fire the ended event at the
  // correct time. Don't propagate silence until the state is FINISHED.
  return !IsPlayingOrScheduled();
}

void AudioBufferSourceHandler::HandleStoppableSourceNode() {
  DCHECK(Context()->IsAudioThread());

  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Can't get the lock, so just return.  It's ok to handle these at a later
    // time; this was just a hint anyway so stopping them a bit later is ok.
    return;
  }

  // If the source node has been scheduled to stop, we can stop the node once
  // the current time reaches that value.  Usually,
  // AudioScheduledSourceHandler::UpdateSchedulingInfo handles stopped nodes,
  // but we can get here if the node is stopped and then disconnected.  Then
  // UpdateSchedulingInfo never gets a chance to finish the node.

  if (end_time_ != AudioScheduledSourceHandler::kUnknownTime &&
      Context()->currentTime() > end_time_) {
    Finish();
    return;
  }

  // If the source node is not looping, and we have a buffer, we can determine
  // when the source would stop playing.  This is intended to handle the
  // (uncommon) scenario where start() has been called but is never connected to
  // the destination (directly or indirectly).  By stopping the node, the node
  // can be collected.  Otherwise, the node will never get collected, leaking
  // memory.
  //
  // If looping was ever done (m_didSetLooping = true), give up.  We can't
  // easily determine how long we looped so we don't know the actual duration
  // thus far, so don't try to do anything fancy.
  double min_playback_rate = GetMinPlaybackRate();
  bool is_finite = !DidSetLooping() || is_duration_given_;
  if (is_finite && Buffer() && IsPlayingOrScheduled() &&
      min_playback_rate > 0) {
    // Adjust the duration to include the playback rate. Only need to account
    // for rate < 1 which makes the sound last longer.  For rate >= 1, the
    // source stops sooner, but that's ok.
    double source_duration =
        is_duration_given_ ? grain_duration_ : Buffer()->duration();
    double actual_duration = source_duration / min_playback_rate;

    double stop_time = start_time_ + actual_duration;

    // See crbug.com/478301. If a source node is started via start(), the source
    // may not start at that time but one quantum (128 frames) later.  But we
    // compute the stop time based on the start time and the duration, so we end
    // up stopping one quantum early.  Thus, add a little extra time; we just
    // need to stop the source sometime after it should have stopped if it
    // hadn't already.  We don't need to be super precise on when to stop.
    double extra_stop_time =
        kExtraStopFrames / static_cast<double>(Context()->sampleRate());

    stop_time += extra_stop_time;
    if (Context()->currentTime() > stop_time) {
      // The context time has passed the time when the source nodes should have
      // stopped playing. Stop the node now and deref it.  Deliver the onended
      // event too, to match what Firefox does.
      Finish();
    }
  }
}

}  // namespace blink
