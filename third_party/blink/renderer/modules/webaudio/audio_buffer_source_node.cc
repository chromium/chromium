/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

const double kDefaultGrainDuration = 0.020;  // 20ms

// Arbitrary upper limit on playback rate.
// Higher than expected rates can be useful when playing back oversampled
// buffers to minimize linear interpolation aliasing.
const double kMaxRate = 1024;

AudioBufferSourceHandler::AudioBufferSourceHandler(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& playback_rate,
    AudioParamHandler& detune)
    : AudioScheduledSourceHandler(kNodeTypeAudioBufferSource,
                                  node,
                                  sample_rate),
      playback_rate_(&playback_rate),
      detune_(&detune),
      is_looping_(false),
      did_set_looping_(false),
      loop_start_(0),
      loop_end_(0),
      virtual_read_index_(0),
      is_grain_(false),
      grain_offset_(0.0),
      grain_duration_(kDefaultGrainDuration),
      min_playback_rate_(1.0),
      buffer_has_been_set_(false) {
  // Default to mono. A call to setBuffer() will set the number of output
  // channels to that of the buffer.
  AddOutput(1);

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
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized()) {
    output_bus->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    if (!Buffer()) {
      output_bus->Zero();
      return;
    }

    // After calling setBuffer() with a buffer having a different number of
    // channels, there can in rare cases be a slight delay before the output bus
    // is updated to the new number of channels because of use of tryLocks() in
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

    for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i)
      destination_channels_[i] = output_bus->Channel(i)->MutableData();

    // Render by reading directly from the buffer.
    if (!RenderFromBuffer(output_bus, quantum_frame_offset,
                          buffer_frames_to_process, start_time_offset)) {
      output_bus->Zero();
      return;
    }

    output_bus->ClearSilentFlag();
  } else {
    // Too bad - the tryLock() failed.  We must be in the middle of changing
    // buffers and were already outputting silence anyway.
    output_bus->Zero();
  }
}

// Returns true if we're finished.
bool AudioBufferSourceHandler::RenderSilenceAndFinishIfNotLooping(
    AudioBus*,
    unsigned index,
    uint32_t frames_to_process) {
  if (!Loop()) {
    // If we're not looping, then stop playing when we get to the end.

    if (frames_to_process > 0) {
      // We're not looping and we've reached the end of the sample data, but we
      // still need to provide more output, so generate silence for the
      // remaining.
      for (unsigned i = 0; i < NumberOfChannels(); ++i)
        memset(destination_channels_[i] + index, 0,
               sizeof(float) * frames_to_process);
    }

    Finish();
    return true;
  }
  return false;
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

  unsigned number_of_channels = this->NumberOfChannels();
  unsigned bus_number_of_channels = bus->NumberOfChannels();

  bool channel_count_good =
      number_of_channels && number_of_channels == bus_number_of_channels;
  DCHECK(channel_count_good);

  // Sanity check destinationFrameOffset, numberOfFrames.
  size_t destination_length = bus->length();

  DCHECK_LE(destination_length, audio_utilities::kRenderQuantumFrames);
  DCHECK_LE(number_of_frames, audio_utilities::kRenderQuantumFrames);

  DCHECK_LE(destination_frame_offset, destination_length);
  DCHECK_LE(destination_frame_offset + number_of_frames, destination_length);

  // Potentially zero out initial frames leading up to the offset.
  if (destination_frame_offset) {
    for (unsigned i = 0; i < number_of_channels; ++i)
      memset(destination_channels_[i], 0,
             sizeof(float) * destination_frame_offset);
  }

  // Offset the pointers to the correct offset frame.
  unsigned write_index = destination_frame_offset;

  uint32_t buffer_length = shared_buffer_->length();
  double buffer_sample_rate = shared_buffer_->sampleRate();

  // Avoid converting from time to sample-frames twice by computing
  // the grain end time first before computing the sample frame.
  unsigned end_frame =
      is_grain_
          ? base::saturated_cast<uint32_t>(audio_utilities::TimeToSampleFrame(
                grain_offset_ + grain_duration_, buffer_sample_rate))
          : buffer_length;

  // Do some sanity checking.
  if (end_frame > buffer_length)
    end_frame = buffer_length;

  // If the .loop attribute is true, then values of
  // m_loopStart == 0 && m_loopEnd == 0 implies that we should use the entire
  // buffer as the loop, otherwise use the loop values in m_loopStart and
  // m_loopEnd.
  double virtual_end_frame = end_frame;
  double virtual_delta_frames = end_frame;

  if (Loop() && (loop_start_ || loop_end_) && loop_start_ >= 0 &&
      loop_end_ > 0 && loop_start_ < loop_end_) {
    // Convert from seconds to sample-frames.
    double loop_start_frame = loop_start_ * shared_buffer_->sampleRate();
    double loop_end_frame = loop_end_ * shared_buffer_->sampleRate();

    virtual_end_frame = std::min(loop_end_frame, virtual_end_frame);
    virtual_delta_frames = virtual_end_frame - loop_start_frame;
  }

  // If we're looping and the offset (virtualReadIndex) is past the end of the
  // loop, wrap back to the beginning of the loop. For other cases, nothing
  // needs to be done.
  if (Loop() && virtual_read_index_ >= virtual_end_frame) {
    virtual_read_index_ =
        (loop_start_ < 0) ? 0 : (loop_start_ * shared_buffer_->sampleRate());
    virtual_read_index_ =
        std::min(virtual_read_index_, static_cast<double>(buffer_length - 1));
  }

  double computed_playback_rate = ComputePlaybackRate();

  // Sanity check that our playback rate isn't larger than the loop size.
  if (computed_playback_rate > virtual_delta_frames)
    return false;

  // Get local copy.
  double virtual_read_index = virtual_read_index_;

  // Adjust the read index by the start_time_offset (compensated by the playback
  // rate) because we always start output on a frame boundary with interpolation
  // if necessary.
  if (start_time_offset < 0) {
    if (computed_playback_rate != 0) {
      virtual_read_index +=
          std::abs(start_time_offset * computed_playback_rate);
    }
  }

  // Render loop - reading from the source buffer to the destination using
  // linear interpolation.
  int frames_to_process = number_of_frames;

  const float** source_channels = source_channels_.get();
  float** destination_channels = destination_channels_.get();

  DCHECK_GE(virtual_read_index, 0);
  DCHECK_GE(virtual_delta_frames, 0);
  DCHECK_GE(virtual_end_frame, 0);

  // Optimize for the very common case of playing back with
  // computedPlaybackRate == 1.  We can avoid the linear interpolation.
  if (computed_playback_rate == 1 &&
      virtual_read_index == floor(virtual_read_index) &&
      virtual_delta_frames == floor(virtual_delta_frames) &&
      virtual_end_frame == floor(virtual_end_frame)) {
    unsigned read_index = static_cast<unsigned>(virtual_read_index);
    unsigned delta_frames = static_cast<unsigned>(virtual_delta_frames);
    end_frame = static_cast<unsigned>(virtual_end_frame);

    while (frames_to_process > 0) {
      int frames_to_end = end_frame - read_index;
      int frames_this_time = std::min(frames_to_process, frames_to_end);
      frames_this_time = std::max(0, frames_this_time);

      DCHECK_LE(write_index + frames_this_time, destination_length);
      DCHECK_LE(read_index + frames_this_time, buffer_length);

      for (unsigned i = 0; i < number_of_channels; ++i)
        memcpy(destination_channels[i] + write_index,
               source_channels[i] + read_index,
               sizeof(float) * frames_this_time);

      write_index += frames_this_time;
      read_index += frames_this_time;
      frames_to_process -= frames_this_time;

      // It can happen that framesThisTime is 0. DCHECK that we will actually
      // exit the loop in this case.  framesThisTime is 0 only if
      // readIndex >= endFrame;
      DCHECK(frames_this_time ? true : read_index >= end_frame);

      // Wrap-around.
      if (read_index >= end_frame) {
        read_index -= delta_frames;
        if (RenderSilenceAndFinishIfNotLooping(bus, write_index,
                                               frames_to_process))
          break;
      }
    }
    virtual_read_index = read_index;
  } else {
    while (frames_to_process--) {
      unsigned read_index = static_cast<unsigned>(virtual_read_index);
      double interpolation_factor = virtual_read_index - read_index;

      // For linear interpolation we need the next sample-frame too.
      unsigned read_index2 = read_index + 1;
      if (read_index2 >= buffer_length) {
        if (Loop()) {
          // Make sure to wrap around at the end of the buffer.
          read_index2 = static_cast<unsigned>(virtual_read_index + 1 -
                                              virtual_delta_frames);
        } else {
          read_index2 = read_index;
        }
      }

      // Final sanity check on buffer access.
      // FIXME: as an optimization, try to get rid of this inner-loop check and
      // put assertions and guards before the loop.
      if (read_index >= buffer_length || read_index2 >= buffer_length)
        break;

      // Linear interpolation.
      for (unsigned i = 0; i < number_of_channels; ++i) {
        float* destination = destination_channels[i];
        const float* source = source_channels[i];
        double sample;

        if (read_index == read_index2 && read_index >= 1) {
          // We're at the end of the buffer, so just linearly extrapolate from
          // the last two samples.
          double sample1 = source[read_index - 1];
          double sample2 = source[read_index];
          sample = sample2 + (sample2 - sample1) * interpolation_factor;
        } else {
          double sample1 = source[read_index];
          double sample2 = source[read_index2];
          sample = (1.0 - interpolation_factor) * sample1 +
                   interpolation_factor * sample2;
        }
        destination[write_index] = clampTo<float>(sample);
      }
      write_index++;

      virtual_read_index += computed_playback_rate;

      // Wrap-around, retaining sub-sample position since virtualReadIndex is
      // floating-point.
      if (virtual_read_index >= virtual_end_frame) {
        virtual_read_index -= virtual_delta_frames;
        if (RenderSilenceAndFinishIfNotLooping(bus, write_index,
                                               frames_to_process))
          break;
      }
    }
  }

  bus->ClearSilentFlag();

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
  BaseAudioContext::GraphAutoLocker context_locker(Context());

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);

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

    source_channels_ = std::make_unique<const float* []>(number_of_channels);
    destination_channels_ = std::make_unique<float* []>(number_of_channels);

    for (unsigned i = 0; i < number_of_channels; ++i) {
      source_channels_[i] =
          static_cast<float*>(shared_buffer_->channels()[i].Data());
    }

    // If this is a grain (as set by a previous call to start()), validate the
    // grain parameters now since it wasn't validated when start was called
    // (because there was no buffer then).
    if (is_grain_)
      ClampGrainParameters(shared_buffer_.get());
  }

  virtual_read_index_ = 0;
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

  grain_offset_ = clampTo(grain_offset_, 0.0, buffer_duration);

  // If the duration was not explicitly given, use the buffer duration to set
  // the grain duration. Otherwise, we want to use the user-specified value, of
  // course.
  if (!is_duration_given_)
    grain_duration_ = buffer_duration - grain_offset_;

  if (is_duration_given_ && Loop()) {
    // We're looping a grain with a grain duration specified. Schedule the loop
    // to stop after grainDuration seconds after starting, possibly running the
    // loop multiple times if grainDuration is larger than the buffer duration.
    // The net effect is as if the user called stop(when + grainDuration).
    grain_duration_ =
        clampTo(grain_duration_, 0.0, std::numeric_limits<double>::infinity());
    end_time_ = start_time_ + grain_duration_;
  } else {
    grain_duration_ =
        clampTo(grain_duration_, 0.0, buffer_duration - grain_offset_);
  }

  // We call timeToSampleFrame here since at playbackRate == 1 we don't want to
  // go through linear interpolation at a sub-sample position since it will
  // degrade the quality. When aligned to the sample-frame the playback will be
  // identical to the PCM data stored in the buffer. Since playbackRate == 1 is
  // very common, it's worth considering quality.
  virtual_read_index_ = audio_utilities::TimeToSampleFrame(
      grain_offset_, shared_buffer_->sampleRate());
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
  MutexLocker process_locker(process_lock_);

  is_duration_given_ = is_duration_given;
  is_grain_ = true;
  grain_offset_ = grain_offset;
  grain_duration_ = grain_duration;

  // If |when| < currentTime, the source must start now according to the spec.
  // So just set startTime to currentTime in this case to start the source now.
  start_time_ = std::max(when, Context()->currentTime());

  if (Buffer())
    ClampGrainParameters(Buffer());

  SetPlaybackState(SCHEDULED_STATE);
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
  final_playback_rate *= pow(2, detune_->FinalValue() / 1200);

  // Sanity check the total rate.  It's very important that the resampler not
  // get any bad rate values.
  final_playback_rate = clampTo(final_playback_rate, 0.0, kMaxRate);

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
  return !IsPlayingOrScheduled() || HasFinished() || !shared_buffer_.get();
}

void AudioBufferSourceHandler::HandleStoppableSourceNode() {
  DCHECK(Context()->IsAudioThread());
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
  if (!DidSetLooping() && Buffer() && IsPlayingOrScheduled() &&
      min_playback_rate > 0) {
    // Adjust the duration to include the playback rate. Only need to account
    // for rate < 1 which makes the sound last longer.  For rate >= 1, the
    // source stops sooner, but that's ok.
    double actual_duration = Buffer()->duration() / min_playback_rate;

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

// ----------------------------------------------------------------
AudioBufferSourceNode::AudioBufferSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      playback_rate_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioBufferSourcePlaybackRate,
          1.0,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed)),
      detune_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioBufferSourceDetune,
          0.0,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed)) {
  SetHandler(AudioBufferSourceHandler::Create(*this, context.sampleRate(),
                                              playback_rate_->Handler(),
                                              detune_->Handler()));
}

AudioBufferSourceNode* AudioBufferSourceNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<AudioBufferSourceNode>(context);
}

AudioBufferSourceNode* AudioBufferSourceNode::Create(
    BaseAudioContext* context,
    AudioBufferSourceOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  AudioBufferSourceNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  if (options->hasBuffer())
    node->setBuffer(options->buffer(), exception_state);
  node->detune()->setValue(options->detune());
  node->setLoop(options->loop());
  node->setLoopEnd(options->loopEnd());
  node->setLoopStart(options->loopStart());
  node->playbackRate()->setValue(options->playbackRate());

  return node;
}

void AudioBufferSourceNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(playback_rate_);
  visitor->Trace(detune_);
  visitor->Trace(buffer_);
  AudioScheduledSourceNode::Trace(visitor);
}

AudioBufferSourceHandler& AudioBufferSourceNode::GetAudioBufferSourceHandler()
    const {
  return static_cast<AudioBufferSourceHandler&>(Handler());
}

AudioBuffer* AudioBufferSourceNode::buffer() const {
  return buffer_.Get();
}

void AudioBufferSourceNode::setBuffer(AudioBuffer* new_buffer,
                                      ExceptionState& exception_state) {
  GetAudioBufferSourceHandler().SetBuffer(new_buffer, exception_state);
  if (!exception_state.HadException())
    buffer_ = new_buffer;
}

AudioParam* AudioBufferSourceNode::playbackRate() const {
  return playback_rate_;
}

AudioParam* AudioBufferSourceNode::detune() const {
  return detune_;
}

bool AudioBufferSourceNode::loop() const {
  return GetAudioBufferSourceHandler().Loop();
}

void AudioBufferSourceNode::setLoop(bool loop) {
  GetAudioBufferSourceHandler().SetLoop(loop);
}

double AudioBufferSourceNode::loopStart() const {
  return GetAudioBufferSourceHandler().LoopStart();
}

void AudioBufferSourceNode::setLoopStart(double loop_start) {
  GetAudioBufferSourceHandler().SetLoopStart(loop_start);
}

double AudioBufferSourceNode::loopEnd() const {
  return GetAudioBufferSourceHandler().LoopEnd();
}

void AudioBufferSourceNode::setLoopEnd(double loop_end) {
  GetAudioBufferSourceHandler().SetLoopEnd(loop_end);
}

void AudioBufferSourceNode::start(ExceptionState& exception_state) {
  GetAudioBufferSourceHandler().Start(0, exception_state);
}

void AudioBufferSourceNode::start(double when,
                                  ExceptionState& exception_state) {
  GetAudioBufferSourceHandler().Start(when, exception_state);
}

void AudioBufferSourceNode::start(double when,
                                  double grain_offset,
                                  ExceptionState& exception_state) {
  GetAudioBufferSourceHandler().Start(when, grain_offset, exception_state);
}

void AudioBufferSourceNode::start(double when,
                                  double grain_offset,
                                  double grain_duration,
                                  ExceptionState& exception_state) {
  GetAudioBufferSourceHandler().Start(when, grain_offset, grain_duration,
                                      exception_state);
}

void AudioBufferSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(detune_);
  GraphTracer().DidCreateAudioParam(playback_rate_);
}

void AudioBufferSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(detune_);
  GraphTracer().WillDestroyAudioParam(playback_rate_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
