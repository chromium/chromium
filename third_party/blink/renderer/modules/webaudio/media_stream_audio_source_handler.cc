// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_handler.h"

#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Default to stereo. This could change depending on the format of the
// MediaStream's audio track.
constexpr unsigned kDefaultNumberOfOutputChannels = 2;

}  // namespace

MediaStreamAudioSourceHandler::MediaStreamAudioSourceHandler(
    AudioNode& node,
    std::unique_ptr<AudioSourceProvider> audio_source_provider)
    : AudioHandler(kNodeTypeMediaStreamAudioSource,
                   node,
                   node.context()->sampleRate()),
      audio_source_provider_(std::move(audio_source_provider)) {
  SendLogMessage(__func__, "");
  AddOutput(kDefaultNumberOfOutputChannels);

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
  SendLogMessage(
      __func__,
      String::Format("({number_of_channels=%u}, {source_sample_rate=%0.f})",
                     number_of_channels, source_sample_rate));

  {
    base::AutoLock locker(process_lock_);
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
      SendLogMessage(
          __func__,
          String::Format("=> (ERROR: invalid channel count requested)"));
      return;
    }

    // Checks for invalid sample rate.
    if (source_sample_rate != Context()->sampleRate()) {
      source_number_of_channels_ = 0;
      SendLogMessage(
          __func__,
          String::Format("=> (ERROR: invalid sample rate requested)"));
      return;
    }

    source_number_of_channels_ = number_of_channels;
  }

  DeferredTaskHandler::GraphAutoLocker graph_locker(Context());
  Output(0).SetNumberOfChannels(number_of_channels);
}

void MediaStreamAudioSourceHandler::Process(uint32_t number_of_frames) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "MediaStreamAudioSourceHandler::Process", "this",
               reinterpret_cast<void*>(this), "number_of_frames",
               number_of_frames);

  AudioBus* output_bus = Output(0).Bus();

  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    if (source_number_of_channels_ != output_bus->NumberOfChannels()) {
      output_bus->Zero();
      return;
    }
    audio_source_provider_.get()->ProvideInput(
        output_bus, base::checked_cast<int>(number_of_frames));
    if (!is_processing_) {
      SendLogMessage(__func__, String::Format("({number_of_frames=%u})",
                                              number_of_frames));
      SendLogMessage(
          __func__,
          String::Format("=> (audio source is now alive and audio frames are "
                         "sent to the output)"));
      is_processing_ = true;
    }
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                 "MediaStreamAudioSourceHandler::Process TryLock failed");
    // If we fail to acquire the lock, it means setFormat() is running. So
    // output silence.
    output_bus->Zero();
  }
}

void MediaStreamAudioSourceHandler::SendLogMessage(
    const char* const function_name,
    const String& message) {
  WebRtcLogMessage(String::Format("[WA]MSASH::%s %s [this=0x%" PRIXPTR "]",
                                  function_name, message.Utf8().c_str(),
                                  reinterpret_cast<uintptr_t>(this))
                       .Utf8());
}

}  // namespace blink
