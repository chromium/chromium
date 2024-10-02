// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/convolver_handler.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_convolver_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/reverb.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// Note about empirical tuning:
// The maximum FFT size affects reverb performance and accuracy.
// If the reverb is single-threaded and processes entirely in the real-time
// audio thread, it's important not to make this too high.  In this case 8192 is
// a good value.  But, the Reverb object is multi-threaded, so we want this as
// high as possible without losing too much accuracy.  Very large FFTs will have
// worse phase errors. Given these constraints 32768 is a good compromise.
constexpr unsigned kMaxFftSize = 32768;

constexpr unsigned kDefaultNumberOfInputChannels = 2;
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

ConvolverHandler::ConvolverHandler(AudioNode& node, float sample_rate)
    : AudioHandler(kNodeTypeConvolver, node, sample_rate) {
  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  // Node-specific default mixing rules.
  channel_count_ = kDefaultNumberOfInputChannels;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kClampedMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  Initialize();

  // Until something is connected, we're not actively processing, so disable
  // outputs so that we produce a single channel of silence.  The graph lock is
  // needed to be able to disable outputs.
  DeferredTaskHandler::GraphAutoLocker context_locker(Context());

  DisableOutputs();
}

scoped_refptr<ConvolverHandler> ConvolverHandler::Create(AudioNode& node,
                                                         float sample_rate) {
  return base::AdoptRef(new ConvolverHandler(node, sample_rate));
}

ConvolverHandler::~ConvolverHandler() {
  Uninitialize();
}

void ConvolverHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  // Synchronize with possible dynamic changes to the impulse response.
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    if (!IsInitialized() || !reverb_) {
      output_bus->Zero();
    } else {
      // Process using the convolution engine.
      // Note that we can handle the case where nothing is connected to the
      // input, in which case we'll just feed silence into the convolver.
      // FIXME:  If we wanted to get fancy we could try to factor in the 'tail
      // time' and stop processing once the tail dies down if
      // we keep getting fed silence.
      scoped_refptr<AudioBus> input_bus = Input(0).Bus();
      reverb_->Process(input_bus.get(), output_bus, frames_to_process);
    }
  } else {
    // Too bad - the tryLock() failed.  We must be in the middle of setting a
    // new impulse response.
    output_bus->Zero();
  }
}

void ConvolverHandler::SetBuffer(AudioBuffer* buffer,
                                 ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!buffer) {
    DeferredTaskHandler::GraphAutoLocker context_locker(Context());
    base::AutoLock locker(process_lock_);
    reverb_.reset();
    shared_buffer_ = nullptr;
    return;
  }

  if (buffer->sampleRate() != Context()->sampleRate()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The buffer sample rate of " + String::Number(buffer->sampleRate()) +
            " does not match the context rate of " +
            String::Number(Context()->sampleRate()) + " Hz.");
    return;
  }

  unsigned number_of_channels = buffer->numberOfChannels();
  uint32_t buffer_length = buffer->length();

  // The current implementation supports only 1-, 2-, or 4-channel impulse
  // responses, with the 4-channel response being interpreted as true-stereo
  // (see Reverb class).
  bool is_channel_count_good = number_of_channels == 1 ||
                               number_of_channels == 2 ||
                               number_of_channels == 4;

  if (!is_channel_count_good) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The buffer must have 1, 2, or 4 channels, not " +
            String::Number(number_of_channels));
    return;
  }

  {
    // Get some statistics on the size of the impulse response.
    UMA_HISTOGRAM_LONG_TIMES("WebAudio.ConvolverNode.ImpulseResponseLength",
                             base::Seconds(buffer->duration()));
  }

  // Wrap the AudioBuffer by an AudioBus. It's an efficient pointer set and not
  // a memcpy().  This memory is simply used in the Reverb constructor and no
  // reference to it is kept for later use in that class.
  scoped_refptr<AudioBus> buffer_bus =
      AudioBus::Create(number_of_channels, buffer_length, false);

  // Check to see if any of the channels have been transferred.  Note that an
  // AudioBuffer cannot be created with a length of 0, so if any channel has a
  // length of 0, it was transferred.
  bool any_buffer_detached = false;
  for (unsigned i = 0; i < number_of_channels; ++i) {
    if (buffer->getChannelData(i)->length() == 0) {
      any_buffer_detached = true;
      break;
    }
  }

  if (any_buffer_detached) {
    // If any channel is detached, we're supposed to treat it as if all were.
    // This means the buffer effectively has length 0, which is the same as if
    // no buffer were given.
    DeferredTaskHandler::GraphAutoLocker context_locker(Context());
    base::AutoLock locker(process_lock_);
    reverb_.reset();
    shared_buffer_ = nullptr;
    return;
  }

  for (unsigned i = 0; i < number_of_channels; ++i) {
    buffer_bus->SetChannelMemory(i, buffer->getChannelData(i)->Data(),
                                 buffer_length);
  }

  buffer_bus->SetSampleRate(buffer->sampleRate());

  // Create the reverb with the given impulse response.
  std::unique_ptr<Reverb> reverb = std::make_unique<Reverb>(
      buffer_bus.get(), GetDeferredTaskHandler().RenderQuantumFrames(),
      kMaxFftSize, Context() && Context()->HasRealtimeConstraint(), normalize_);

  {
    // The context must be locked since changing the buffer can
    // re-configure the number of channels that are output.
    DeferredTaskHandler::GraphAutoLocker context_locker(Context());

    // Synchronize with process().
    base::AutoLock locker(process_lock_);
    reverb_ = std::move(reverb);
    shared_buffer_ = buffer->CreateSharedAudioBuffer();
    if (buffer) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(ComputeNumberOfOutputChannels(
          Input(0).NumberOfChannels(), shared_buffer_->numberOfChannels()));
    }
  }
}

bool ConvolverHandler::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double ConvolverHandler::TailTime() const {
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    return reverb_ ? reverb_->ImpulseResponseLength() /
                         static_cast<double>(Context()->sampleRate())
                   : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double ConvolverHandler::LatencyTime() const {
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    return reverb_ ? reverb_->LatencyFrames() /
                         static_cast<double>(Context()->sampleRate())
                   : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

unsigned ConvolverHandler::ComputeNumberOfOutputChannels(
    unsigned input_channels,
    unsigned response_channels) const {
  // The number of output channels for a Convolver must be one or two.
  // And can only be one if there's a mono source and a mono response
  // buffer.
  return ClampTo(std::max(input_channels, response_channels), 1, 2);
}

void ConvolverHandler::SetChannelCount(unsigned channel_count,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channelCount must be 1 or 2
  if (channel_count == 1 || channel_count == 2) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      UpdateChannelsForInputs();
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<uint32_t>(
            "channelCount", channel_count, 1,
            ExceptionMessages::kInclusiveBound, 2,
            ExceptionMessages::kInclusiveBound));
  }
}

void ConvolverHandler::SetChannelCountMode(V8ChannelCountMode::Enum mode,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  V8ChannelCountMode::Enum old_mode = InternalChannelCountMode();

  // The channelCountMode cannot be "max".  For a convolver node, the
  // number of input channels must be 1 or 2 (see
  // https://webaudio.github.io/web-audio-api/#audionode-channelcount-constraints)
  // and "max" would be incompatible with that.
  if (mode == V8ChannelCountMode::Enum::kMax) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ConvolverNode: channelCountMode cannot be changed to 'max'");
    new_channel_count_mode_ = old_mode;
  } else if (mode == V8ChannelCountMode::Enum::kExplicit ||
             mode == V8ChannelCountMode::Enum::kClampedMax) {
    new_channel_count_mode_ = mode;
  } else {
    NOTREACHED();
  }

  if (new_channel_count_mode_ != old_mode) {
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
  }
}

void ConvolverHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK(input);
  DCHECK_EQ(input, &Input(0));

  bool has_shared_buffer = false;
  unsigned number_of_channels = 1;
  bool lock_successfully_acquired = false;

  // TODO(crbug.com/1447093): Check what to do when the lock cannot be acquired.
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    lock_successfully_acquired = true;
    has_shared_buffer = !!shared_buffer_;
    if (has_shared_buffer)
      number_of_channels = shared_buffer_->numberOfChannels();
  }

  if (has_shared_buffer || !lock_successfully_acquired) {
    unsigned number_of_output_channels = ComputeNumberOfOutputChannels(
        input->NumberOfChannels(), number_of_channels);

    if (IsInitialized() &&
        number_of_output_channels != Output(0).NumberOfChannels()) {
      // We're already initialized but the channel count has changed.
      Uninitialize();
    }

    if (!IsInitialized()) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(number_of_output_channels);
      Initialize();
    }
  }

  // Update the input's internal bus if needed.
  AudioHandler::CheckNumberOfChannelsForInput(input);
}

}  // namespace blink
