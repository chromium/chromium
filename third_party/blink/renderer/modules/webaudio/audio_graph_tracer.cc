// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"

namespace blink {

const char AudioGraphTracer::kSupplementName[] = "AudioGraphTracer";

void AudioGraphTracer::ProvideAudioGraphTracerTo(Page& page) {
  page.ProvideSupplement(MakeGarbageCollected<AudioGraphTracer>(page));
}

AudioGraphTracer::AudioGraphTracer(Page& page) : Supplement(page) {}

void AudioGraphTracer::Trace(Visitor* visitor) const {
  visitor->Trace(inspector_agent_);
  visitor->Trace(contexts_);
  Supplement<Page>::Trace(visitor);
}

void AudioGraphTracer::SetInspectorAgent(InspectorWebAudioAgent* agent) {
  inspector_agent_ = agent;
  if (!inspector_agent_) {
    return;
  }
  for (const auto& context : contexts_) {
    inspector_agent_->DidCreateBaseAudioContext(context);
  }
}

void AudioGraphTracer::DidCreateBaseAudioContext(BaseAudioContext* context) {
  DCHECK(!contexts_.Contains(context));

  contexts_.insert(context);
  if (inspector_agent_) {
    inspector_agent_->DidCreateBaseAudioContext(context);
  }
}

void AudioGraphTracer::WillDestroyBaseAudioContext(BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  contexts_.erase(context);
  if (inspector_agent_) {
    inspector_agent_->WillDestroyBaseAudioContext(context);
  }
}

void AudioGraphTracer::DidChangeBaseAudioContext(BaseAudioContext* context) {
  DCHECK(contexts_.Contains(context));

  if (inspector_agent_) {
    inspector_agent_->DidChangeBaseAudioContext(context);
  }
}

BaseAudioContext* AudioGraphTracer::GetContextById(String contextId) {
  for (const auto& context : contexts_) {
    if (context->Uuid() == contextId) {
      return context.Get();
    }
  }

  return nullptr;
}

void AudioGraphTracer::DidCreateAudioListener(AudioListener* listener) {
  if (inspector_agent_) {
    inspector_agent_->DidCreateAudioListener(listener);
  }
}

void AudioGraphTracer::WillDestroyAudioListener(AudioListener* listener) {
  if (inspector_agent_) {
    inspector_agent_->WillDestroyAudioListener(listener);
  }
}

void AudioGraphTracer::DidCreateAudioNode(AudioNode* node) {
  if (inspector_agent_) {
    inspector_agent_->DidCreateAudioNode(node);
  }
}

void AudioGraphTracer::WillDestroyAudioNode(AudioNode* node) {
  if (inspector_agent_ && contexts_.Contains(node->context())) {
    inspector_agent_->WillDestroyAudioNode(node);
  }
}

void AudioGraphTracer::DidCreateAudioParam(AudioParam* param) {
  if (inspector_agent_) {
    inspector_agent_->DidCreateAudioParam(param);
  }
}

void AudioGraphTracer::WillDestroyAudioParam(AudioParam* param) {
  if (inspector_agent_ && contexts_.Contains(param->Context())) {
    inspector_agent_->WillDestroyAudioParam(param);
  }
}

void AudioGraphTracer::DidConnectNodes(AudioNode* source_node,
                                       AudioNode* destination_node,
                                       unsigned source_output_index,
                                       unsigned destination_input_index) {
  if (inspector_agent_) {
    inspector_agent_->DidConnectNodes(source_node, destination_node,
        source_output_index, destination_input_index);
  }
}

void AudioGraphTracer::DidDisconnectNodes(
    AudioNode* source_node,
    AudioNode* destination_node,
    unsigned source_output_index,
    unsigned destination_input_index) {
  if (inspector_agent_) {
    inspector_agent_->DidDisconnectNodes(source_node, destination_node,
        source_output_index, destination_input_index);
  }
}

void AudioGraphTracer::DidConnectNodeParam(
    AudioNode* source_node,
    AudioParam* destination_param,
    unsigned source_output_index) {
  if (inspector_agent_) {
    inspector_agent_->DidConnectNodeParam(source_node, destination_param,
        source_output_index);
  }
}

void AudioGraphTracer::DidDisconnectNodeParam(
    AudioNode* source_node,
    AudioParam* destination_param,
    unsigned source_output_index) {
  if (inspector_agent_) {
    inspector_agent_->DidDisconnectNodeParam(source_node, destination_param,
        source_output_index);
  }
}

AudioGraphTracer* AudioGraphTracer::FromPage(Page* page) {
  return Supplement<Page>::From<AudioGraphTracer>(page);
}

AudioGraphTracer* AudioGraphTracer::FromWindow(const LocalDOMWindow& window) {
  return AudioGraphTracer::FromPage(window.GetFrame()->GetPage());
}

}  // namespace blink
