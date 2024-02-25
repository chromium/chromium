// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/stereo_panner_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_stereo_panner_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/stereo_panner.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

constexpr double kDefaultPanValue = 0.0;
constexpr float kMinPanValue = -1.0f;
constexpr float kMaxPanValue = 1.0f;

}  // namespace

StereoPannerNode::StereoPannerNode(BaseAudioContext& context)
    : AudioNode(context),
      pan_(AudioParam::Create(context,
                              Uuid(),
                              AudioParamHandler::kParamTypeStereoPannerPan,
                              kDefaultPanValue,
                              AudioParamHandler::AutomationRate::kAudio,
                              AudioParamHandler::AutomationRateMode::kVariable,
                              kMinPanValue,
                              kMaxPanValue)) {
  SetHandler(StereoPannerHandler::Create(*this, context.sampleRate(),
                                         pan_->Handler()));
}

StereoPannerNode* StereoPannerNode::Create(BaseAudioContext& context,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<StereoPannerNode>(context);
}

StereoPannerNode* StereoPannerNode::Create(BaseAudioContext* context,
                                           const StereoPannerOptions* options,
                                           ExceptionState& exception_state) {
  StereoPannerNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->pan()->setValue(options->pan());

  return node;
}

void StereoPannerNode::Trace(Visitor* visitor) const {
  visitor->Trace(pan_);
  AudioNode::Trace(visitor);
}

AudioParam* StereoPannerNode::pan() const {
  return pan_.Get();
}

void StereoPannerNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(pan_);
}

void StereoPannerNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(pan_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
