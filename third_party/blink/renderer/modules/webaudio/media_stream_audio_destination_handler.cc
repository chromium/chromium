// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_handler.h"

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Channel counts greater than 8 are ignored by some audio tracks/sinks (see
// WebAudioMediaStreamSource), so we set a limit here to avoid anything that
// could cause a crash.
constexpr uint32_t kMaxChannelCountSupported = 8;

}  // namespace

MediaStreamAudioDestinationHandler::MediaStreamAudioDestinationHandler(
    AudioNode& node,
    uint32_t number_of_channels)
    : AudioHandler(kNodeTypeMediaStreamAudioDestination,
                   node,
                   node.context()->sampleRate()),
      source_(static_cast<MediaStreamAudioDestinationNode&>(node).source()),
      mix_bus_(
          AudioBus::Create(number_of_channels,
                           GetDeferredTaskHandler().RenderQuantumFrames())) {
  AddInput();
  SendLogMessage(__func__, "");
  source_.Lock()->SetAudioFormat(static_cast<int>(number_of_channels),
                                 node.context()->sampleRate());
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "MediaStreamAudioDestinationHandler::Process");

  // Conform the input bus into the internal mix bus, which represents
  // MediaStreamDestination's channel count.

  // Synchronize with possible dynamic changes to the channel count.
  base::AutoTryLock try_locker(process_lock_);

  auto source = source_.Lock();

  // If we can get the lock, we can process normally by updating the
  // mix bus to a new channel count, if needed.  If not, just use the
  // old mix bus to do the mixing; we'll update the bus next time
  // around.
  if (try_locker.is_acquired()) {
    unsigned count = ChannelCount();
    if (count != mix_bus_->NumberOfChannels()) {
      mix_bus_ = AudioBus::Create(
          count, GetDeferredTaskHandler().RenderQuantumFrames());
      // setAudioFormat has an internal lock.  This can cause audio to
      // glitch.  This is outside of our control.
      source->SetAudioFormat(static_cast<int>(count), Context()->sampleRate());
    }
  }

  mix_bus_->CopyFrom(*Input(0).Bus());

  // consumeAudio has an internal lock (also used by setAudioFormat).
  // This can cause audio to glitch.  This is outside of our control.
  source->ConsumeAudio(mix_bus_.get(), static_cast<int>(number_of_frames));
}

void MediaStreamAudioDestinationHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Currently the maximum channel count supported for this node is 8,
  // which is constrained by source_ (WebAudioMediaStreamSource). Although
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
  // needs to update mix_bus_.
  base::AutoLock locker(process_lock_);

  AudioHandler::SetChannelCount(channel_count, exception_state);
}

uint32_t MediaStreamAudioDestinationHandler::MaxChannelCount() const {
  return kMaxChannelCountSupported;
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

  DCHECK_EQ(input, &Input(0));

  AudioHandler::CheckNumberOfChannelsForInput(input);

  UpdatePullStatusIfNeeded();
}

void MediaStreamAudioDestinationHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  unsigned number_of_input_connections =
      Input(0).NumberOfRenderingConnections();
  if (number_of_input_connections && !need_automatic_pull_) {
    // When a MediaStreamAudioDestinationHandler is not connected to any
    // downstream node while still connected from upstream node(s), add it to
    // the context's automatic pull list.
    Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
    need_automatic_pull_ = true;
  } else if (!number_of_input_connections && need_automatic_pull_) {
    // The MediaStreamAudioDestinationHandler is connected to nothing; remove it
    // from the context's automatic pull list.
    Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
    need_automatic_pull_ = false;
  }
}

void MediaStreamAudioDestinationHandler::SendLogMessage(
    const char* const function_name,
    const String& message) {
  WebRtcLogMessage(String::Format("[WA]MSADH::%s %s [this=0x%" PRIXPTR "]",
                                  function_name, message.Utf8().c_str(),
                                  reinterpret_cast<uintptr_t>(this))
                       .Utf8());
}

}  // namespace blink
