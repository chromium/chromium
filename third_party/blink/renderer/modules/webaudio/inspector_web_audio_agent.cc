// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"

#include <memory>

#include "third_party/blink/renderer/bindings/modules/v8/v8_automation_rate.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"

namespace blink {

namespace {

String GetContextTypeEnum(BaseAudioContext* context) {
  return context->HasRealtimeConstraint()
      ? protocol::WebAudio::ContextTypeEnum::Realtime
      : protocol::WebAudio::ContextTypeEnum::Offline;
}

String GetContextStateEnum(BaseAudioContext* context) {
  switch (context->ContextState()) {
    case BaseAudioContext::AudioContextState::kSuspended:
      return protocol::WebAudio::ContextStateEnum::Suspended;
    case BaseAudioContext::AudioContextState::kRunning:
      return protocol::WebAudio::ContextStateEnum::Running;
    case BaseAudioContext::AudioContextState::kClosed:
      return protocol::WebAudio::ContextStateEnum::Closed;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

// Strips "Node" from the node name string. For example, "GainNode" will return
// "Gain".
String StripNodeSuffix(const String& nodeName) {
  return nodeName.EndsWith("Node") ? nodeName.Left(nodeName.length() - 4)
                                   : "Unknown";
}

// Strips out the prefix and returns the actual parameter name. If the name
// does not match `NodeName.ParamName` pattern, returns "Unknown" instead.
String StripParamPrefix(const String& paramName) {
  Vector<String> name_tokens;
  paramName.Split('.', name_tokens);
  return name_tokens.size() == 2 ? name_tokens.at(1) : "Unknown";
}

}  // namespace

InspectorWebAudioAgent::InspectorWebAudioAgent(Page* page)
    : page_(page),
      enabled_(&agent_state_, /*default_value=*/false) {
}

InspectorWebAudioAgent::~InspectorWebAudioAgent() = default;

void InspectorWebAudioAgent::Restore() {
  if (!enabled_.Get()) {
    return;
  }

  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(this);
}

protocol::Response InspectorWebAudioAgent::enable() {
  if (enabled_.Get()) {
    return protocol::Response::Success();
  }
  enabled_.Set(true);
  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorWebAudioAgent::disable() {
  if (!enabled_.Get()) {
    return protocol::Response::Success();
  }
  enabled_.Clear();
  AudioGraphTracer* graph_tracer = AudioGraphTracer::FromPage(page_);
  graph_tracer->SetInspectorAgent(nullptr);
  return protocol::Response::Success();
}

protocol::Response InspectorWebAudioAgent::getRealtimeData(
    const protocol::WebAudio::GraphObjectId& contextId,
    std::unique_ptr<ContextRealtimeData>* out_data) {
  auto* const graph_tracer = AudioGraphTracer::FromPage(page_);
  if (!enabled_.Get()) {
    return protocol::Response::ServerError("Enable agent first.");
  }

  BaseAudioContext* context = graph_tracer->GetContextById(contextId);
  if (!context) {
    return protocol::Response::ServerError(
        "Cannot find BaseAudioContext with such id.");
  }

  if (!context->HasRealtimeConstraint()) {
    return protocol::Response::ServerError(
        "ContextRealtimeData is only avaliable for an AudioContext.");
  }

  // The realtime metric collection is only for AudioContext.
  AudioCallbackMetric metric =
      static_cast<AudioContext*>(context)->GetCallbackMetric();
  *out_data = ContextRealtimeData::create()
          .setCurrentTime(context->currentTime())
          .setRenderCapacity(metric.render_capacity)
          .setCallbackIntervalMean(metric.mean_callback_interval)
          .setCallbackIntervalVariance(metric.variance_callback_interval)
          .build();
  return protocol::Response::Success();
}

void InspectorWebAudioAgent::DidCreateBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextCreated(BuildProtocolContext(context));
}

void InspectorWebAudioAgent::WillDestroyBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextWillBeDestroyed(context->Uuid());
}

void InspectorWebAudioAgent::DidChangeBaseAudioContext(
    BaseAudioContext* context) {
  GetFrontend()->contextChanged(BuildProtocolContext(context));
}

void InspectorWebAudioAgent::DidCreateAudioListener(AudioListener* listener) {
  GetFrontend()->audioListenerCreated(
      protocol::WebAudio::AudioListener::create()
          .setListenerId(listener->Uuid())
          .setContextId(listener->ParentUuid())
          .build());
}

void InspectorWebAudioAgent::WillDestroyAudioListener(AudioListener* listener) {
  GetFrontend()->audioListenerWillBeDestroyed(
      listener->ParentUuid(), listener->Uuid());
}

void InspectorWebAudioAgent::DidCreateAudioNode(AudioNode* node) {
  GetFrontend()->audioNodeCreated(
      protocol::WebAudio::AudioNode::create()
          .setNodeId(node->Uuid())
          .setNodeType(StripNodeSuffix(node->GetNodeName()))
          .setNumberOfInputs(node->numberOfInputs())
          .setNumberOfOutputs(node->numberOfOutputs())
          .setChannelCount(node->channelCount())
          .setChannelCountMode(node->channelCountMode().AsString())
          .setChannelInterpretation(node->channelInterpretation().AsString())
          .setContextId(node->ParentUuid())
          .build());
}

void InspectorWebAudioAgent::WillDestroyAudioNode(AudioNode* node) {
  GetFrontend()->audioNodeWillBeDestroyed(node->ParentUuid(), node->Uuid());
}

void InspectorWebAudioAgent::DidCreateAudioParam(AudioParam* param) {
  GetFrontend()->audioParamCreated(
      protocol::WebAudio::AudioParam::create()
          .setParamId(param->Uuid())
          .setParamType(StripParamPrefix(param->GetParamName()))
          .setRate(param->automationRate().AsString())
          .setDefaultValue(param->defaultValue())
          .setMinValue(param->minValue())
          .setMaxValue(param->maxValue())
          .setContextId(param->Context()->Uuid())
          .setNodeId(param->ParentUuid())
          .build());
}

void InspectorWebAudioAgent::WillDestroyAudioParam(AudioParam* param) {
  GetFrontend()->audioParamWillBeDestroyed(
      param->Context()->Uuid(), param->ParentUuid(), param->Uuid());
}

void InspectorWebAudioAgent::DidConnectNodes(
    AudioNode* source_node,
    AudioNode* destination_node,
    int32_t source_output_index,
    int32_t destination_input_index) {
  GetFrontend()->nodesConnected(
      source_node->ParentUuid(),
      source_node->Uuid(),
      destination_node->Uuid(),
      source_output_index,
      destination_input_index);
}

void InspectorWebAudioAgent::DidDisconnectNodes(
    AudioNode* source_node,
    AudioNode* destination_node,
    int32_t source_output_index,
    int32_t destination_input_index) {
  GetFrontend()->nodesDisconnected(
      source_node->ParentUuid(),
      source_node->Uuid(),
      destination_node ? destination_node->Uuid() : String(),
      source_output_index,
      destination_input_index);
}

void InspectorWebAudioAgent::DidConnectNodeParam(
    AudioNode* source_node,
    AudioParam* destination_param,
    int32_t source_output_index) {
  GetFrontend()->nodeParamConnected(
      source_node->ParentUuid(),
      source_node->Uuid(),
      destination_param->Uuid(),
      source_output_index);
}

void InspectorWebAudioAgent::DidDisconnectNodeParam(
    AudioNode* source_node,
    AudioParam* destination_param,
    int32_t source_output_index) {
  GetFrontend()->nodeParamDisconnected(
      source_node->ParentUuid(),
      source_node->Uuid(),
      destination_param->Uuid(),
      source_output_index);
}

std::unique_ptr<protocol::WebAudio::BaseAudioContext>
InspectorWebAudioAgent::BuildProtocolContext(BaseAudioContext* context) {
  return protocol::WebAudio::BaseAudioContext::create()
      .setContextId(context->Uuid())
      .setContextType(GetContextTypeEnum(context))
      .setContextState(GetContextStateEnum(context))
      .setCallbackBufferSize(context->CallbackBufferSize())
      .setMaxOutputChannelCount(context->MaxChannelCount())
      .setSampleRate(context->sampleRate())
      .build();
}

void InspectorWebAudioAgent::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
