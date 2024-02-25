// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/constant_source_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_constant_source_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/constant_source_handler.h"

namespace blink {

namespace {

constexpr double kDefaultOffsetValue = 1.0;

}  // namespace

ConstantSourceNode::ConstantSourceNode(BaseAudioContext& context)
    : AudioScheduledSourceNode(context),
      offset_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeConstantSourceOffset,
          kDefaultOffsetValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(ConstantSourceHandler::Create(*this, context.sampleRate(),
                                           offset_->Handler()));
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<ConstantSourceNode>(context);
}

ConstantSourceNode* ConstantSourceNode::Create(
    BaseAudioContext* context,
    const ConstantSourceOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  ConstantSourceNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->offset()->setValue(options->offset());

  return node;
}

void ConstantSourceNode::Trace(Visitor* visitor) const {
  visitor->Trace(offset_);
  AudioScheduledSourceNode::Trace(visitor);
}

ConstantSourceHandler& ConstantSourceNode::GetConstantSourceHandler() const {
  return static_cast<ConstantSourceHandler&>(Handler());
}

AudioParam* ConstantSourceNode::offset() {
  return offset_.Get();
}

void ConstantSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(offset_);
}

void ConstantSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(offset_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
