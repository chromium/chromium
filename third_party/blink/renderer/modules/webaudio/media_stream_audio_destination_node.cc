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

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"

#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_center.h"
#include "third_party/blink/renderer/platform/uuid.h"
#include "third_party/blink/renderer/platform/wtf/locker.h"

namespace blink {

// WebAudioCapturerSource ignores the channel count beyond 8, so we set the
// block here to avoid anything can cause the crash.
static const unsigned long kMaxChannelCount = 8;

MediaStreamAudioDestinationHandler::MediaStreamAudioDestinationHandler(
    AudioNode& node,
    size_t number_of_channels)
    : AudioBasicInspectorHandler(kNodeTypeMediaStreamAudioDestination,
                                 node,
                                 node.context()->sampleRate(),
                                 number_of_channels),
      mix_bus_(AudioBus::Create(number_of_channels,
                                audio_utilities::kRenderQuantumFrames)) {
  source_ = MediaStreamSource::Create("WebAudio-" + CreateCanonicalUUIDString(),
                                      MediaStreamSource::kTypeAudio,
                                      "MediaStreamAudioDestinationNode", false,
                                      MediaStreamSource::kReadyStateLive, true);
  MediaStreamSourceVector audio_sources;
  audio_sources.push_back(source_.Get());
  MediaStreamSourceVector video_sources;
  stream_ = MediaStream::Create(
      node.context()->GetExecutionContext(),
      MediaStreamDescriptor::Create(audio_sources, video_sources));
  MediaStreamCenter::Instance().DidCreateMediaStreamAndTracks(
      stream_->Descriptor());

  source_->SetAudioFormat(number_of_channels, node.context()->sampleRate());

  SetInternalChannelCountMode(kExplicit);
  Initialize();
}

scoped_refptr<MediaStreamAudioDestinationHandler>
MediaStreamAudioDestinationHandler::Create(AudioNode& node,
                                           size_t number_of_channels) {
  return base::AdoptRef(
      new MediaStreamAudioDestinationHandler(node, number_of_channels));
}

MediaStreamAudioDestinationHandler::~MediaStreamAudioDestinationHandler() {
  Uninitialize();
}

void MediaStreamAudioDestinationHandler::Process(size_t number_of_frames) {
  // Conform the input bus into the internal mix bus, which represents
  // MediaStreamDestination's channel count.

  // Synchronize with possible dynamic changes to the channel count.
  MutexTryLocker try_locker(process_lock_);

  // If we can get the lock, we can process normally by updating the
  // mix bus to a new channel count, if needed.  If not, just use the
  // old mix bus to do the mixing; we'll update the bus next time
  // around.
  if (try_locker.Locked()) {
    unsigned count = ChannelCount();
    if (count != mix_bus_->NumberOfChannels()) {
      mix_bus_ = AudioBus::Create(count, audio_utilities::kRenderQuantumFrames);
      // setAudioFormat has an internal lock.  This can cause audio to
      // glitch.  This is outside of our control.
      source_->SetAudioFormat(count, Context()->sampleRate());
    }
  }

  mix_bus_->CopyFrom(*Input(0).Bus());

  // consumeAudio has an internal lock (also used by setAudioFormat).
  // This can cause audio to glitch.  This is outside of our control.
  source_->ConsumeAudio(mix_bus_.get(), number_of_frames);
}

void MediaStreamAudioDestinationHandler::SetChannelCount(
    unsigned long channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Currently the maximum channel count supported for this node is 8,
  // which is constrained by m_source (WebAudioCapturereSource). Although
  // it has its own safety check for the excessive channels, throwing an
  // exception here is useful to developers.
  if (channel_count < 1 || channel_count > MaxChannelCount()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound, MaxChannelCount(),
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // Synchronize changes in the channel count with process() which
  // needs to update m_mixBus.
  MutexLocker locker(process_lock_);

  AudioHandler::SetChannelCount(channel_count, exception_state);
}

unsigned long MediaStreamAudioDestinationHandler::MaxChannelCount() const {
  return kMaxChannelCount;
}

// ----------------------------------------------------------------

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(
    AudioContext& context,
    size_t number_of_channels)
    : AudioBasicInspectorNode(context) {
  SetHandler(
      MediaStreamAudioDestinationHandler::Create(*this, number_of_channels));
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext& context,
    size_t number_of_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  return new MediaStreamAudioDestinationNode(context, number_of_channels);
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext* context,
    const AudioNodeOptions& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default to stereo; |options| will update it approriately if needed.
  MediaStreamAudioDestinationNode* node =
      new MediaStreamAudioDestinationNode(*context, 2);

  // Need to handle channelCount here ourselves because the upper
  // limit is different from the normal AudioNode::setChannelCount
  // limit of 32.  Error messages will sometimes show the wrong
  // limits.
  if (options.hasChannelCount())
    node->setChannelCount(options.channelCount(), exception_state);

  node->HandleChannelOptions(options, exception_state);

  return node;
}

MediaStream* MediaStreamAudioDestinationNode::stream() const {
  return static_cast<MediaStreamAudioDestinationHandler&>(Handler()).Stream();
}

}  // namespace blink
