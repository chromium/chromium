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
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

MediaStreamAudioSourceHandler::MediaStreamAudioSourceHandler(
    AudioNode& node,
    std::unique_ptr<AudioSourceProvider> audio_source_provider)
    : AudioHandler(kNodeTypeMediaStreamAudioSource,
                   node,
                   node.context()->sampleRate()),
      audio_source_provider_(std::move(audio_source_provider)),
      source_number_of_channels_(0) {
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
  if (number_of_channels != source_number_of_channels_ ||
      source_sample_rate != Context()->sampleRate()) {
    // The sample-rate must be equal to the context's sample-rate.
    if (!number_of_channels ||
        number_of_channels > BaseAudioContext::MaxNumberOfChannels() ||
        source_sample_rate != Context()->sampleRate()) {
      // process() will generate silence for these uninitialized values.
      DLOG(ERROR) << "setFormat(" << number_of_channels << ", "
                  << source_sample_rate << ") - unhandled format change";
      source_number_of_channels_ = 0;
      return;
    }

    // Synchronize with process().
    MutexLocker locker(process_lock_);

    source_number_of_channels_ = number_of_channels;

    {
      // The context must be locked when changing the number of output channels.
      BaseAudioContext::GraphAutoLocker context_locker(Context());

      // Do any necesssary re-configuration to the output's number of channels.
      Output(0).SetNumberOfChannels(number_of_channels);
    }
  }
}

void MediaStreamAudioSourceHandler::Process(uint32_t number_of_frames) {
  AudioBus* output_bus = Output(0).Bus();

  if (!GetAudioSourceProvider()) {
    output_bus->Zero();
    return;
  }

  if (source_number_of_channels_ != output_bus->NumberOfChannels()) {
    output_bus->Zero();
    return;
  }

  // Use a tryLock() to avoid contention in the real-time audio thread.
  // If we fail to acquire the lock then the MediaStream must be in the middle
  // of a format change, so we output silence in this case.
  MutexTryLocker try_locker(process_lock_);
  if (try_locker.Locked()) {
    GetAudioSourceProvider()->ProvideInput(output_bus, number_of_frames);
  } else {
    // We failed to acquire the lock.
    output_bus->Zero();
  }
}

// ----------------------------------------------------------------

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

  MediaStreamTrackVector audio_tracks = media_stream.getAudioTracks();
  if (audio_tracks.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "MediaStream has no audio track");
    return nullptr;
  }

  // Find the first track, which is the track whose id comes first given a
  // lexicographic ordering of the code units of the track id.
  MediaStreamTrack* audio_track = audio_tracks[0];
  for (auto track : audio_tracks) {
    if (CodeUnitCompareLessThan(track->id(), audio_track->id())) {
      audio_track = track;
    }
  }
  std::unique_ptr<AudioSourceProvider> provider =
      audio_track->CreateWebAudioSource(context.sampleRate());

  MediaStreamAudioSourceNode* node =
      MakeGarbageCollected<MediaStreamAudioSourceNode>(
          context, media_stream, audio_track, std::move(provider));

  if (!node)
    return nullptr;

  // TODO(hongchan): Only stereo streams are supported right now. We should be
  // able to accept multi-channel streams.
  node->SetFormat(2, context.sampleRate());
  // context keeps reference until node is disconnected
  context.NotifySourceNodeStartedProcessing(node);

  return node;
}

MediaStreamAudioSourceNode* MediaStreamAudioSourceNode::Create(
    AudioContext* context,
    const MediaStreamAudioSourceOptions* options,
    ExceptionState& exception_state) {
  return Create(*context, *options->mediaStream(), exception_state);
}

void MediaStreamAudioSourceNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(audio_track_);
  visitor->Trace(media_stream_);
  AudioSourceProviderClient::Trace(visitor);
  AudioNode::Trace(visitor);
}

MediaStreamAudioSourceHandler&
MediaStreamAudioSourceNode::GetMediaStreamAudioSourceHandler() const {
  return static_cast<MediaStreamAudioSourceHandler&>(Handler());
}

MediaStream* MediaStreamAudioSourceNode::getMediaStream() const {
  return media_stream_;
}

void MediaStreamAudioSourceNode::SetFormat(uint32_t number_of_channels,
                                           float source_sample_rate) {
  GetMediaStreamAudioSourceHandler().SetFormat(number_of_channels,
                                               source_sample_rate);
}

bool MediaStreamAudioSourceNode::HasPendingActivity() const {
  // As long as the context is running, this node has activity.
  return (context()->ContextState() == BaseAudioContext::kRunning);
}

void MediaStreamAudioSourceNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaStreamAudioSourceNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
