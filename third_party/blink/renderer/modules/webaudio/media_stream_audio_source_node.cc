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

#include <memory>
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_audio_source_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

MediaStreamAudioSourceHandler::MediaStreamAudioSourceHandler(
    AudioNode& node,
    std::unique_ptr<AudioSourceProvider> audio_source_provider)
    : AudioHandler(kNodeTypeMediaStreamAudioSource,
                   node,
                   node.context()->sampleRate()),
      audio_source_provider_(std::move(audio_source_provider)) {
  // Default to stereo. This could change depending on the format of the
  // MediaStream's audio track.
  AddOutput(2);

  Initialize();
}

scoped_refptr<MediaStreamAudioSourceHandler>
MediaStreamAudioSourceHandler::Create(
    AudioNode& node,
    std::unique_ptr<AudioSourceProvider> audio_source_provider) {
  return base::AdoptRef(new MediaStreamAudioSourceHandler(
      node, std::move(audio_source_provider)));
}

MediaStreamAudioSourceHandler::~MediaStreamAudioSourceHandler() {
  Uninitialize();
}

void MediaStreamAudioSourceHandler::SetFormat(uint32_t number_of_channels,
                                              float source_sample_rate) {
  DCHECK(IsMainThread());

  {
    MutexLocker locker(process_lock_);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                "MediaStreamAudioSourceHandler::SetFormat under lock");

    // If the channel count and the sample rate match, nothing to do here.
    if (number_of_channels == source_number_of_channels_ &&
        source_sample_rate == Context()->sampleRate()) {
      return;
    }

    // Checks for invalid channel count.
    if (number_of_channels == 0 ||
        number_of_channels > BaseAudioContext::MaxNumberOfChannels()) {
      source_number_of_channels_ = 0;
      DLOG(ERROR) << "MediaStreamAudioSourceHandler::setFormat - "
                  << "invalid channel count requested ("
                  << number_of_channels << ")";
      return;
    }

    // Checks for invalid sample rate.
    if (source_sample_rate != Context()->sampleRate()) {
      source_number_of_channels_ = 0;
      DLOG(ERROR) << "MediaStreamAudioSourceHandler::setFormat - "
                  << "invalid sample rate requested ("
                  << source_sample_rate << ")";
      return;
    }

    source_number_of_channels_ = number_of_channels;
  }

  BaseAudioContext::GraphAutoLocker graph_locker(Context());
  Output(0).SetNumberOfChannels(number_of_channels);
}

void MediaStreamAudioSourceHandler::Process(uint32_t number_of_frames) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "MediaStreamAudioSourceHandler::Process", "this", this,
               "number_of_frames", number_of_frames);

  AudioBus* output_bus = Output(0).Bus();

  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    if (source_number_of_channels_ != output_bus->NumberOfChannels()) {
      output_bus->Zero();
      return;
    }
    audio_source_provider_.get()->ProvideInput(output_bus, number_of_frames);
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                 "MediaStreamAudioSourceHandler::Process TryLock failed");
    // If we fail to acquire the lock, it means setFormat() is running. So
    // output silence.
    output_bus->Zero();
  }
}

// -----------------------------------------------------------------------------

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(
    AudioContext& context,
    MediaStream& media_stream,
    MediaStreamTrack* audio_track,
    std::unique_ptr<AudioSourceProvider> audio_source_provider)
    : AudioNode(context),
      audio_track_(audio_track),
      media_stream_(media_stream) {
  SetHandler(MediaStreamAudioSourceHandler::Create(
      *this, std::move(audio_source_provider)));
}

MediaStreamAudioSourceNode* MediaStreamAudioSourceNode::Create(
    AudioContext& context,
    MediaStream& media_stream,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state))
    return nullptr;

  // The constructor algorithm:
  // https://webaudio.github.io/web-audio-api/#mediastreamaudiosourcenode

  // 1.24.1. Step 1 & 2.
  MediaStreamTrackVector audio_tracks = media_stream.getAudioTracks();
  if (audio_tracks.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "MediaStream has no audio track");
    return nullptr;
  }

  // 1.24.1. Step 3: Sort the elements in tracks based on their id attribute
  // using an ordering on sequences of code unit values.
  // (See: https://infra.spec.whatwg.org/#code-unit)
  MediaStreamTrack* audio_track = audio_tracks[0];
  for (auto track : audio_tracks) {
    if (CodeUnitCompareLessThan(track->id(), audio_track->id()))
      audio_track = track;
  }

  // 1.24.1. Step 5: The step is out of order because the constructor needs
  // this provider, which is [[input track]] from the spec.
  std::unique_ptr<AudioSourceProvider> provider =
      audio_track->CreateWebAudioSource(context.sampleRate());

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

}  // namespace blink
