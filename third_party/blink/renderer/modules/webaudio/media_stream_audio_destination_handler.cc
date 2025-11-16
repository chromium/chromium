// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_handler.h"

#include <inttypes.h>

#include "base/synchronization/lock.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_destination_consumer.h"

namespace blink {

namespace {

// Channel counts greater than 8 are ignored by some audio tracks/sinks (see
// WebAudioMediaStreamSource), so we set a limit here to avoid anything that
// could cause a crash.
constexpr uint32_t kMaxChannelCountSupported = 8;

}  // namespace

MediaStreamAudioDestinationHandler::MediaStreamAudioDestinationHandler(
    AudioNode& node,
    uint32_t number_of_channels,
    WebAudioDestinationConsumer* webaudio_consumer)
    : AudioHandler(NodeType::kNodeTypeMediaStreamAudioDestination,
                   node,
                   node.context()->sampleRate()),
      mix_bus_(
          AudioBus::Create(number_of_channels,
                           GetDeferredTaskHandler().RenderQuantumFrames())) {
  SendLogMessage(__func__, "");

  AddInput();
  consumer_bus_wrapper_.ReserveInitialCapacity(kMaxChannelCountSupported);
  SetConsumer(webaudio_consumer,
              static_cast<int>(number_of_channels),
              node.context()->sampleRate());
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);
  Initialize();
}

scoped_refptr<MediaStreamAudioDestinationHandler>
MediaStreamAudioDestinationHandler::Create(
    AudioNode& node, uint32_t number_of_channels,
    WebAudioDestinationConsumer* webaudio_consumer) {
  return base::AdoptRef(
      new MediaStreamAudioDestinationHandler(
          node, number_of_channels, webaudio_consumer));
}

MediaStreamAudioDestinationHandler::~MediaStreamAudioDestinationHandler() {
  RemoveConsumer();
  Uninitialize();
}

void MediaStreamAudioDestinationHandler::Process(uint32_t number_of_frames) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "MediaStreamAudioDestinationHandler::Process");

  // Conform the input bus into the internal mix bus, which represents
  // MediaStreamDestination's channel count.

  const unsigned old_channel_count = mix_bus_->NumberOfChannels();
  unsigned new_channel_count = old_channel_count;
  {
    // Synchronize with possible dynamic changes to the channel count.
    base::AutoTryLock try_locker(process_lock_);

    // If we can get the lock, we can process normally by updating the
    // mix bus to a new channel count, if needed.  If not, just use the
    // old mix bus to do the mixing; we'll update the bus next time
    // around.
    if (try_locker.is_acquired()) {
      new_channel_count = ChannelCount();
    }
  }

  if (new_channel_count != old_channel_count) {
    mix_bus_ = AudioBus::Create(new_channel_count,
                                GetDeferredTaskHandler().RenderQuantumFrames());
    {
      base::AutoLock consumer_locker(consumer_lock_);
      if (destination_consumer_) {
        TRACE_EVENT2("webaudio", "MediaStreamAudioDestinationHandler::Process",
                     "old_channel_count", old_channel_count,
                     "new_channel_count", new_channel_count);
        destination_consumer_->SetFormat(new_channel_count,
                                         Context()->sampleRate());
      }
    }
  }

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();
  const AudioBus* bus_to_consume = input_bus.get();

  // If the input bus has the same number of channels as the mix bus we can use
  // it directly and avoid a copy.
  if (bus_to_consume->NumberOfChannels() != mix_bus_->NumberOfChannels()) {
    mix_bus_->CopyFrom(*input_bus);
    bus_to_consume = mix_bus_.get();
  }

  ConsumeAudio(bus_to_consume, static_cast<int>(number_of_frames));
}

void MediaStreamAudioDestinationHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Currently the maximum channel count supported for this node is 8,
  // which is constrained by source_ (WebAudioMediaStreamSource). Although
  // it has its own safety check for the excessive channels, throwing an
  // exception here is useful to developers.
  if (channel_count < 1 || channel_count > kMaxChannelCountSupported) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound, kMaxChannelCountSupported,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  // Synchronize changes in the channel count with process() which
  // needs to update mix_bus_.
  base::AutoLock locker(process_lock_);

  AudioHandler::SetChannelCount(channel_count, exception_state);
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

  Context()->GetDeferredTaskHandler().UpdatePullStatusWithFeatureCheck(this);
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

void MediaStreamAudioDestinationHandler::SetConsumer(
    WebAudioDestinationConsumer* destination_consumer,
    int number_of_channels,
    float sample_rate) {
  if (!destination_consumer) {
    return;
  }

  base::AutoLock locker(consumer_lock_);
  destination_consumer_ = destination_consumer;
  destination_consumer_->SetFormat(number_of_channels, sample_rate);
}

bool MediaStreamAudioDestinationHandler::RemoveConsumer() {
  base::AutoLock locker(consumer_lock_);
  if (!destination_consumer_) {
    return false;
  }

  destination_consumer_ = nullptr;
  return true;
}

void MediaStreamAudioDestinationHandler::ConsumeAudio(
    const AudioBus* const input_bus,
    int number_of_frames) {
  if (!input_bus) {
    return;
  }

  base::AutoTryLock try_locker(consumer_lock_);
  if (try_locker.is_acquired() && destination_consumer_) {
    unsigned number_of_channels = input_bus->NumberOfChannels();
    if (consumer_bus_wrapper_.size() != number_of_channels) {
      consumer_bus_wrapper_.resize(number_of_channels);
    }
    for (unsigned i = 0; i < number_of_channels; ++i) {
      consumer_bus_wrapper_[i] = input_bus->Channel(i)->Data();
    }

    destination_consumer_->ConsumeAudio(consumer_bus_wrapper_,
                                        number_of_frames);
  }
}

}  // namespace blink
