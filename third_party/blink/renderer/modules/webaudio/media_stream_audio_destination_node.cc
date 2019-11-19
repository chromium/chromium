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
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

void DidCreateMediaStreamAndTracks(MediaStreamDescriptor* stream) {
  for (uint32_t i = 0; i < stream->NumberOfAudioComponents(); ++i)
    MediaStreamUtils::DidCreateMediaStreamTrack(stream->AudioComponent(i));

  for (uint32_t i = 0; i < stream->NumberOfVideoComponents(); ++i)
    MediaStreamUtils::DidCreateMediaStreamTrack(stream->VideoComponent(i));
}

}  // namespace

// WebAudioCapturerSource ignores the channel count beyond 8, so we set the
// block here to avoid anything can cause the crash.
static const uint32_t kMaxChannelCount = 8;

MediaStreamAudioDestinationHandler::MediaStreamAudioDestinationHandler(
    AudioNode& node,
    uint32_t number_of_channels)
    : AudioBasicInspectorHandler(kNodeTypeMediaStreamAudioDestination,
                                 node,
                                 node.context()->sampleRate()),
      source_(static_cast<MediaStreamAudioDestinationNode&>(node).source()),
      mix_bus_(AudioBus::Create(number_of_channels,
                                audio_utilities::kRenderQuantumFrames)) {
  source_->SetAudioFormat(number_of_channels, node.context()->sampleRate());
  SetInternalChannelCountMode(kExplicit);
  Initialize();
}

scoped_refptr<MediaStreamAudioDestinationHandler>
MediaStreamAudioDestinationHandler::Create(AudioNode& node,
                                           uint32_t number_of_channels) {
  return base::AdoptRef(
      new MediaStreamAudioDestinationHandler(node, number_of_channels));
}

MediaStreamAudioDestinationHandler::~MediaStreamAudioDestinationHandler() {
  Uninitialize();
}

void MediaStreamAudioDestinationHandler::Process(uint32_t number_of_frames) {
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
    unsigned channel_count,
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

uint32_t MediaStreamAudioDestinationHandler::MaxChannelCount() const {
  return kMaxChannelCount;
}

void MediaStreamAudioDestinationHandler::PullInputs(
    uint32_t frames_to_process) {
  DCHECK_EQ(NumberOfOutputs(), 0u);

  // Just render the input; there's no output for this node.
  Input(0).Pull(nullptr, frames_to_process);
}

void MediaStreamAudioDestinationHandler::CheckNumberOfChannelsForInput(
    AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &this->Input(0));

  AudioHandler::CheckNumberOfChannelsForInput(input);

  UpdatePullStatusIfNeeded();
}

void MediaStreamAudioDestinationHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  unsigned number_of_input_connections =
      Input(0).NumberOfRenderingConnections();
  if (number_of_input_connections && !need_automatic_pull_) {
    // When an AudioBasicInspectorNode is not connected to any downstream node
    // while still connected from upstream node(s), add it to the context's
    // automatic pull list.
    Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
    need_automatic_pull_ = true;
  } else if (!number_of_input_connections && need_automatic_pull_) {
    // The AudioBasicInspectorNode is connected to nothing and is
    // not an AnalyserNode, remove it from the context's automatic
    // pull list.  AnalyserNode's need to be pulled even with no
    // inputs so that the internal state gets updated to hold the
    // right time and FFT data.
    Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
    need_automatic_pull_ = false;
  }
}

// ----------------------------------------------------------------

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(
    AudioContext& context,
    uint32_t number_of_channels)
    : AudioBasicInspectorNode(context),
      source_(MakeGarbageCollected<MediaStreamSource>(
          "WebAudio-" + WTF::CreateCanonicalUUIDString(),
          MediaStreamSource::kTypeAudio,
          "MediaStreamAudioDestinationNode",
          false,
          MediaStreamSource::kReadyStateLive,
          true)),
      stream_(MediaStream::Create(context.GetExecutionContext(),
                                  MakeGarbageCollected<MediaStreamDescriptor>(
                                      MediaStreamSourceVector({source_.Get()}),
                                      MediaStreamSourceVector()))) {
  DidCreateMediaStreamAndTracks(stream_->Descriptor());
  SetHandler(
      MediaStreamAudioDestinationHandler::Create(*this, number_of_channels));
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext& context,
    uint32_t number_of_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<MediaStreamAudioDestinationNode>(
      context, number_of_channels);
}

MediaStreamAudioDestinationNode* MediaStreamAudioDestinationNode::Create(
    AudioContext* context,
    const AudioNodeOptions* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default to stereo; |options| will update it approriately if needed.
  MediaStreamAudioDestinationNode* node =
      MakeGarbageCollected<MediaStreamAudioDestinationNode>(*context, 2);

  // Need to handle channelCount here ourselves because the upper
  // limit is different from the normal AudioNode::setChannelCount
  // limit of 32.  Error messages will sometimes show the wrong
  // limits.
  if (options->hasChannelCount())
    node->setChannelCount(options->channelCount(), exception_state);

  node->HandleChannelOptions(options, exception_state);

  return node;
}

void MediaStreamAudioDestinationNode::Trace(Visitor* visitor) {
  visitor->Trace(stream_);
  visitor->Trace(source_);
  AudioBasicInspectorNode::Trace(visitor);
}

void MediaStreamAudioDestinationNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void MediaStreamAudioDestinationNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
