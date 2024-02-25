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

#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"

#include <algorithm>

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

constexpr double kDefaultPlaybackRateValue = 1.0;
constexpr double kDefaultDetuneValue = 0.0;

}  // namespace

AudioBufferSourceNode::AudioBufferSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      playback_rate_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioBufferSourcePlaybackRate,
          kDefaultPlaybackRateValue,
          AudioParamHandler::AutomationRate::kControl,
          AudioParamHandler::AutomationRateMode::kFixed)),
      detune_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeAudioBufferSourceDetune,
          kDefaultDetuneValue,
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

  if (!node) {
    return nullptr;
  }

  if (options->hasBuffer()) {
    node->setBuffer(options->buffer(), exception_state);
  }
  node->detune()->setValue(options->detune());
  node->setLoop(options->loop());
  node->setLoopEnd(options->loopEnd());
  node->setLoopStart(options->loopStart());
  node->playbackRate()->setValue(options->playbackRate());

  return node;
}

void AudioBufferSourceNode::Trace(Visitor* visitor) const {
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
  if (!exception_state.HadException()) {
    buffer_ = new_buffer;
  }
}

AudioParam* AudioBufferSourceNode::playbackRate() const {
  return playback_rate_.Get();
}

AudioParam* AudioBufferSourceNode::detune() const {
  return detune_.Get();
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
