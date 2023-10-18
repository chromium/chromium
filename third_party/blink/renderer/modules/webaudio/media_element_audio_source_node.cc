/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"

#include <memory>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_element_audio_source_options.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

MediaElementAudioSourceNode::MediaElementAudioSourceNode(
    AudioContext& context,
    HTMLMediaElement& media_element)
    : AudioNode(context),
      ActiveScriptWrappable<MediaElementAudioSourceNode>({}),
      media_element_(&media_element) {
  SetHandler(MediaElementAudioSourceHandler::Create(*this, media_element));
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    AudioContext& context,
    HTMLMediaElement& media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // First check if this media element already has a source node.
  if (media_element.AudioSourceNode()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "HTMLMediaElement already connected "
                                      "previously to a different "
                                      "MediaElementSourceNode.");
    return nullptr;
  }

  MediaElementAudioSourceNode* node =
      MakeGarbageCollected<MediaElementAudioSourceNode>(context, media_element);

  if (node) {
    media_element.SetAudioSourceNode(node);
    // context keeps reference until node is disconnected
    context.NotifySourceNodeStartedProcessing(node);
    if (!context.HasRealtimeConstraint()) {
      Deprecation::CountDeprecation(
          node->GetExecutionContext(),
          WebFeature::kMediaElementSourceOnOfflineContext);
    }
  }

  return node;
}

MediaElementAudioSourceNode* MediaElementAudioSourceNode::Create(
    AudioContext* context,
    const MediaElementAudioSourceOptions* options,
    ExceptionState& exception_state) {
  return Create(*context, *options->mediaElement(), exception_state);
}

MediaElementAudioSourceHandler&
MediaElementAudioSourceNode::GetMediaElementAudioSourceHandler() const {
  return static_cast<MediaElementAudioSourceHandler&>(Handler());
}

HTMLMediaElement* MediaElementAudioSourceNode::mediaElement() const {
  return media_element_.Get();
}

void MediaElementAudioSourceNode::SetFormat(uint32_t number_of_channels,
                                            float sample_rate) {
  GetMediaElementAudioSourceHandler().SetFormat(number_of_channels,
                                                sample_rate);
}

void MediaElementAudioSourceNode::lock() {
  GetMediaElementAudioSourceHandler().lock();
}

void MediaElementAudioSourceNode::unlock() {
  GetMediaElementAudioSourceHandler().unlock();
}

void MediaElementAudioSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaElementAudioSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

bool MediaElementAudioSourceNode::HasPendingActivity() const {
  // The node stays alive as long as the context is running.
  return context()->ContextState() == BaseAudioContext::kRunning;
}

void MediaElementAudioSourceNode::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  AudioSourceProviderClient::Trace(visitor);
  AudioNode::Trace(visitor);
}

}  // namespace blink
