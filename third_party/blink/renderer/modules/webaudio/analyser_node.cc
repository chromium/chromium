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

#include "third_party/blink/renderer/modules/webaudio/analyser_node.h"

#include "third_party/blink/renderer/modules/webaudio/analyser_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

AnalyserHandler::AnalyserHandler(AudioNode& node, float sample_rate)
    : AudioBasicInspectorHandler(kNodeTypeAnalyser, node, sample_rate) {
  channel_count_ = 2;
  AddOutput(1);

  Initialize();
}

scoped_refptr<AnalyserHandler> AnalyserHandler::Create(AudioNode& node,
                                                       float sample_rate) {
  return base::AdoptRef(new AnalyserHandler(node, sample_rate));
}

AnalyserHandler::~AnalyserHandler() {
  Uninitialize();
}

void AnalyserHandler::Process(uint32_t frames_to_process) {
  AudioBus* output_bus = Output(0).Bus();

  if (!IsInitialized()) {
    output_bus->Zero();
    return;
  }

  AudioBus* input_bus = Input(0).Bus();

  // Give the analyser the audio which is passing through this
  // AudioNode.  This must always be done so that the state of the
  // Analyser reflects the current input.
  analyser_.WriteInput(input_bus, frames_to_process);

  if (!Input(0).IsConnected()) {
    // No inputs, so clear the output, and propagate the silence hint.
    output_bus->Zero();
    return;
  }

  // For in-place processing, our override of pullInputs() will just pass the
  // audio data through unchanged if the channel count matches from input to
  // output (resulting in inputBus == outputBus). Otherwise, do an up-mix to
  // stereo.
  if (input_bus != output_bus)
    output_bus->CopyFrom(*input_bus);
}

void AnalyserHandler::SetFftSize(unsigned size,
                                 ExceptionState& exception_state) {
  if (!analyser_.SetFftSize(size)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        (size < RealtimeAnalyser::kMinFFTSize ||
         size > RealtimeAnalyser::kMaxFFTSize)
            ? ExceptionMessages::IndexOutsideRange(
                  "FFT size", size, RealtimeAnalyser::kMinFFTSize,
                  ExceptionMessages::kInclusiveBound,
                  RealtimeAnalyser::kMaxFFTSize,
                  ExceptionMessages::kInclusiveBound)
            : ("The value provided (" + String::Number(size) +
               ") is not a power of two."));
  }
}

void AnalyserHandler::SetMinDecibels(double k,
                                     ExceptionState& exception_state) {
  if (k < MaxDecibels()) {
    analyser_.SetMinDecibels(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("minDecibels", k,
                                                    MaxDecibels()));
  }
}

void AnalyserHandler::SetMaxDecibels(double k,
                                     ExceptionState& exception_state) {
  if (k > MinDecibels()) {
    analyser_.SetMaxDecibels(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("maxDecibels", k,
                                                    MinDecibels()));
  }
}

void AnalyserHandler::SetMinMaxDecibels(double min_decibels,
                                        double max_decibels,
                                        ExceptionState& exception_state) {
  if (min_decibels >= max_decibels) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "maxDecibels (" + String::Number(max_decibels) +
            ") must be greater than or equal to minDecibels " + "( " +
            String::Number(min_decibels) + ").");
    return;
  }
  analyser_.SetMinDecibels(min_decibels);
  analyser_.SetMaxDecibels(max_decibels);
}

void AnalyserHandler::SetSmoothingTimeConstant(
    double k,
    ExceptionState& exception_state) {
  if (k >= 0 && k <= 1) {
    analyser_.SetSmoothingTimeConstant(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "smoothing value", k, 0.0, ExceptionMessages::kInclusiveBound, 1.0,
            ExceptionMessages::kInclusiveBound));
  }
}

void AnalyserHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  if (Output(0).IsConnected()) {
    // When an AudioBasicInspectorNode is connected to a downstream node, it
    // will get pulled by the downstream node, thus remove it from the context's
    // automatic pull list.
    if (need_automatic_pull_) {
      Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
      need_automatic_pull_ = false;
    }
  } else {
    unsigned number_of_input_connections =
        Input(0).NumberOfRenderingConnections();
    // When an AnalyserNode is not connected to any downstream node
    // while still connected from upstream node(s), add it to the context's
    // automatic pull list.
    //
    // But don't remove the AnalyserNode if there are no inputs
    // connected to the node.  The node needs to be pulled so that the
    // internal state is updated with the correct input signal (of
    // zeroes).
    if (number_of_input_connections && !need_automatic_pull_) {
      Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
      need_automatic_pull_ = true;
    }
  }
}

bool AnalyserHandler::RequiresTailProcessing() const {
  // Tail time is always non-zero so tail processing is required.
  return true;
}

double AnalyserHandler::TailTime() const {
  return RealtimeAnalyser::kMaxFFTSize /
         static_cast<double>(Context()->sampleRate());
}
// ----------------------------------------------------------------

AnalyserNode::AnalyserNode(BaseAudioContext& context)
    : AudioBasicInspectorNode(context) {
  SetHandler(AnalyserHandler::Create(*this, context.sampleRate()));
}

AnalyserNode* AnalyserNode::Create(BaseAudioContext& context,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<AnalyserNode>(context);
}

AnalyserNode* AnalyserNode::Create(BaseAudioContext* context,
                                   const AnalyserOptions* options,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  AnalyserNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->setFftSize(options->fftSize(), exception_state);
  node->setSmoothingTimeConstant(options->smoothingTimeConstant(),
                                 exception_state);

  // minDecibels and maxDecibels have default values.  Set both of the values
  // at once.
  node->SetMinMaxDecibels(options->minDecibels(), options->maxDecibels(),
                          exception_state);

  return node;
}

AnalyserHandler& AnalyserNode::GetAnalyserHandler() const {
  return static_cast<AnalyserHandler&>(Handler());
}

unsigned AnalyserNode::fftSize() const {
  return GetAnalyserHandler().FftSize();
}

void AnalyserNode::setFftSize(unsigned size, ExceptionState& exception_state) {
  return GetAnalyserHandler().SetFftSize(size, exception_state);
}

unsigned AnalyserNode::frequencyBinCount() const {
  return GetAnalyserHandler().FrequencyBinCount();
}

void AnalyserNode::setMinDecibels(double min, ExceptionState& exception_state) {
  GetAnalyserHandler().SetMinDecibels(min, exception_state);
}

double AnalyserNode::minDecibels() const {
  return GetAnalyserHandler().MinDecibels();
}

void AnalyserNode::setMaxDecibels(double max, ExceptionState& exception_state) {
  GetAnalyserHandler().SetMaxDecibels(max, exception_state);
}

void AnalyserNode::SetMinMaxDecibels(double min,
                                     double max,
                                     ExceptionState& exception_state) {
  GetAnalyserHandler().SetMinMaxDecibels(min, max, exception_state);
}

double AnalyserNode::maxDecibels() const {
  return GetAnalyserHandler().MaxDecibels();
}

void AnalyserNode::setSmoothingTimeConstant(double smoothing_time,
                                            ExceptionState& exception_state) {
  GetAnalyserHandler().SetSmoothingTimeConstant(smoothing_time,
                                                exception_state);
}

double AnalyserNode::smoothingTimeConstant() const {
  return GetAnalyserHandler().SmoothingTimeConstant();
}

void AnalyserNode::getFloatFrequencyData(NotShared<DOMFloat32Array> array) {
  GetAnalyserHandler().GetFloatFrequencyData(array.View(),
                                             context()->currentTime());
}

void AnalyserNode::getByteFrequencyData(NotShared<DOMUint8Array> array) {
  GetAnalyserHandler().GetByteFrequencyData(array.View(),
                                            context()->currentTime());
}

void AnalyserNode::getFloatTimeDomainData(NotShared<DOMFloat32Array> array) {
  GetAnalyserHandler().GetFloatTimeDomainData(array.View());
}

void AnalyserNode::getByteTimeDomainData(NotShared<DOMUint8Array> array) {
  GetAnalyserHandler().GetByteTimeDomainData(array.View());
}

void AnalyserNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void AnalyserNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
