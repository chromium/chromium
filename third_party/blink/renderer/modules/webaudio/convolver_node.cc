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

#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"
#include <memory>
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/convolver_options.h"
#include "third_party/blink/renderer/platform/audio/reverb.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

// Note about empirical tuning:
// The maximum FFT size affects reverb performance and accuracy.
// If the reverb is single-threaded and processes entirely in the real-time
// audio thread, it's important not to make this too high.  In this case 8192 is
// a good value.  But, the Reverb object is multi-threaded, so we want this as
// high as possible without losing too much accuracy.  Very large FFTs will have
// worse phase errors. Given these constraints 32768 is a good compromise.
const size_t MaxFFTSize = 32768;

namespace blink {

ConvolverHandler::ConvolverHandler(AudioNode& node, float sample_rate)
    : AudioHandler(kNodeTypeConvolver, node, sample_rate), normalize_(true) {
  AddInput();
  AddOutput(1);

  // Node-specific default mixing rules.
  channel_count_ = 2;
  SetInternalChannelCountMode(kClampedMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  Initialize();

  // Until something is connected, we're not actively processing, so disable
  // outputs so that we produce a single channel of silence.  The graph lock is
  // needed to be able to disable outputs.
  BaseAudioContext::GraphAutoLocker context_locker(Context());

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
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    if (!IsInitialized() || !reverb_) {
      output_bus->Zero();
    } else {
      // Process using the convolution engine.
      // Note that we can handle the case where nothing is connected to the
      // input, in which case we'll just feed silence into the convolver.
      // FIXME:  If we wanted to get fancy we could try to factor in the 'tail
      // time' and stop processing once the tail dies down if
      // we keep getting fed silence.
      reverb_->Process(Input(0).Bus(), output_bus, frames_to_process);
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
    BaseAudioContext::GraphAutoLocker context_locker(Context());
    MutexLocker locker(process_lock_);
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
                             base::TimeDelta::FromSecondsD(buffer->duration()));
  }

  // Wrap the AudioBuffer by an AudioBus. It's an efficient pointer set and not
  // a memcpy().  This memory is simply used in the Reverb constructor and no
  // reference to it is kept for later use in that class.
  scoped_refptr<AudioBus> buffer_bus =
      AudioBus::Create(number_of_channels, buffer_length, false);
  for (unsigned i = 0; i < number_of_channels; ++i) {
    buffer_bus->SetChannelMemory(i, buffer->getChannelData(i).View()->Data(),
                                 buffer_length);
  }

  buffer_bus->SetSampleRate(buffer->sampleRate());

  // Create the reverb with the given impulse response.
  std::unique_ptr<Reverb> reverb = std::make_unique<Reverb>(
      buffer_bus.get(), audio_utilities::kRenderQuantumFrames, MaxFFTSize,
      Context() && Context()->HasRealtimeConstraint(), normalize_);

  {
    // The context must be locked since changing the buffer can
    // re-configure the number of channels that are output.
    BaseAudioContext::GraphAutoLocker context_locker(Context());

    // Synchronize with process().
    MutexLocker locker(process_lock_);
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
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked())
    return reverb_ ? reverb_->ImpulseResponseLength() /
                         static_cast<double>(Context()->sampleRate())
                   : 0;
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double ConvolverHandler::LatencyTime() const {
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked())
    return reverb_ ? reverb_->LatencyFrames() /
                         static_cast<double>(Context()->sampleRate())
                   : 0;
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
  return clampTo(std::max(input_channels, response_channels), 1, 2);
}

void ConvolverHandler::SetChannelCount(unsigned channel_count,
                                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channelCount must be 2.
  if (channel_count != 2) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ConvolverNode: channelCount cannot be changed from 2");
  }
}

void ConvolverHandler::SetChannelCountMode(const String& mode,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'clamped-max'.
  if (mode != "clamped-max") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ConvolverNode: channelCountMode cannot be changed from 'clamped-max'");
  }
}

void ConvolverHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK(input);
  DCHECK_EQ(input, &this->Input(0));

  if (shared_buffer_) {
    unsigned number_of_output_channels = ComputeNumberOfOutputChannels(
        input->NumberOfChannels(), shared_buffer_->numberOfChannels());

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
// ----------------------------------------------------------------

ConvolverNode::ConvolverNode(BaseAudioContext& context) : AudioNode(context) {
  SetHandler(ConvolverHandler::Create(*this, context.sampleRate()));
}

ConvolverNode* ConvolverNode::Create(BaseAudioContext& context,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<ConvolverNode>(context);
}

ConvolverNode* ConvolverNode::Create(BaseAudioContext* context,
                                     const ConvolverOptions* options,
                                     ExceptionState& exception_state) {
  ConvolverNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  // It is important to set normalize first because setting the buffer will
  // examing the normalize attribute to see if normalization needs to be done.
  node->setNormalize(!options->disableNormalization());
  if (options->hasBuffer())
    node->setBuffer(options->buffer(), exception_state);
  return node;
}

ConvolverHandler& ConvolverNode::GetConvolverHandler() const {
  return static_cast<ConvolverHandler&>(Handler());
}

AudioBuffer* ConvolverNode::buffer() const {
  return buffer_;
}

void ConvolverNode::setBuffer(AudioBuffer* new_buffer,
                              ExceptionState& exception_state) {
  GetConvolverHandler().SetBuffer(new_buffer, exception_state);
  if (!exception_state.HadException())
    buffer_ = new_buffer;
}

bool ConvolverNode::normalize() const {
  return GetConvolverHandler().Normalize();
}

void ConvolverNode::setNormalize(bool normalize) {
  GetConvolverHandler().SetNormalize(normalize);
}

void ConvolverNode::Trace(Visitor* visitor) {
  visitor->Trace(buffer_);
  AudioNode::Trace(visitor);
}

void ConvolverNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ConvolverNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
