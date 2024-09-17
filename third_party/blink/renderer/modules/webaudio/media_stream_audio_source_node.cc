/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"

#include <inttypes.h>

#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_audio_source_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_handler.h"

namespace blink {

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(
    AudioContext& context,
    MediaStream& media_stream,
    MediaStreamTrack* audio_track,
    std::unique_ptr<AudioSourceProvider> audio_source_provider)
    : AudioNode(context),
      ActiveScriptWrappable<MediaStreamAudioSourceNode>({}),
      audio_track_(audio_track),
      media_stream_(media_stream) {
  SetHandler(MediaStreamAudioSourceHandler::Create(
      *this, std::move(audio_source_provider)));
  SendLogMessage(
      __func__,
      String::Format(
          "({audio_track=[kind: %s, id: "
          "%s, label: %s, enabled: "
          "%d, muted: %d]}, {handler=0x%" PRIXPTR "}, [this=0x%" PRIXPTR "])",
          audio_track->kind().Utf8().c_str(), audio_track->id().Utf8().c_str(),
          audio_track->label().Utf8().c_str(), audio_track->enabled(),
          audio_track->muted(), reinterpret_cast<uintptr_t>(&Handler()),
          reinterpret_cast<uintptr_t>(this)));
}

MediaStreamAudioSourceNode* MediaStreamAudioSourceNode::Create(
    AudioContext& context,
    MediaStream& media_stream,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state)) {
    return nullptr;
  }

  // The constructor algorithm:
  // https://webaudio.github.io/web-audio-api/#mediastreamaudiosourcenode

  // 1.24.1. Step 1 & 2.
  MediaStreamTrackVector audio_tracks = media_stream.getAudioTracks();
  if (audio_tracks.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "MediaStream has no audio track");
    return nullptr;
  }

  // 1.24.1. Step 3: Sort the elements in tracks based on their id attribute
  // using an ordering on sequences of code unit values.
  // (See: https://infra.spec.whatwg.org/#code-unit)
  MediaStreamTrack* audio_track = audio_tracks[0];
  for (auto track : audio_tracks) {
    if (CodeUnitCompareLessThan(track->id(), audio_track->id())) {
      audio_track = track;
    }
  }

  // 1.24.1. Step 5: The step is out of order because the constructor needs
  // this provider, which is [[input track]] from the spec.
  std::unique_ptr<AudioSourceProvider> provider =
      audio_track->CreateWebAudioSource(context.sampleRate(),
                                        context.PlatformBufferDuration());

  // 1.24.1. Step 4.
  MediaStreamAudioSourceNode* node =
      MakeGarbageCollected<MediaStreamAudioSourceNode>(
          context, media_stream, audio_track, std::move(provider));

  // Initializes the node with the stereo output channel.
  node->SetFormat(2, context.sampleRate());

  // Lets the context know this source node started.
  context.NotifySourceNodeStartedProcessing(node);

  return node;
}

MediaStreamAudioSourceNode* MediaStreamAudioSourceNode::Create(
    AudioContext* context,
    const MediaStreamAudioSourceOptions* options,
    ExceptionState& exception_state) {
  return Create(*context, *options->mediaStream(), exception_state);
}

void MediaStreamAudioSourceNode::SetFormat(uint32_t number_of_channels,
                                           float source_sample_rate) {
  GetMediaStreamAudioSourceHandler().SetFormat(number_of_channels,
                                               source_sample_rate);
}

void MediaStreamAudioSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaStreamAudioSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

bool MediaStreamAudioSourceNode::HasPendingActivity() const {
  // The node stays alive as long as the context is running. It also will not
  // be collected until the context is suspended or stopped.
  // (See https://crbug.com/937231)
  return context()->ContextState() == BaseAudioContext::kRunning;
}

void MediaStreamAudioSourceNode::Trace(Visitor* visitor) const {
  visitor->Trace(audio_track_);
  visitor->Trace(media_stream_);
  AudioSourceProviderClient::Trace(visitor);
  AudioNode::Trace(visitor);
}

MediaStreamAudioSourceHandler&
MediaStreamAudioSourceNode::GetMediaStreamAudioSourceHandler() const {
  return static_cast<MediaStreamAudioSourceHandler&>(Handler());
}

void MediaStreamAudioSourceNode::SendLogMessage(const char* const function_name,
                                                const String& message) {
  WebRtcLogMessage(
      String::Format("[WA]MSASN::%s %s", function_name, message.Utf8().c_str())
          .Utf8());
}

}  // namespace blink
