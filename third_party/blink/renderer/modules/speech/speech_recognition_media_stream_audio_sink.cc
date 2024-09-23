// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_media_stream_audio_sink.h"

#include <memory>

#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/mojo/mojom/audio_data.mojom-blink.h"
#include "media/mojo/mojom/speech_recognition.mojom-blink.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {
template <>
struct CrossThreadCopier<media::mojom::blink::AudioDataS16Ptr>
    : public CrossThreadCopierByValuePassThrough<
          media::mojom::blink::AudioDataS16Ptr> {};
}  // namespace WTF

namespace blink {

SpeechRecognitionMediaStreamAudioSink::SpeechRecognitionMediaStreamAudioSink(
    ExecutionContext* context,
    mojo::PendingRemote<media::mojom::blink::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    const media::AudioParameters& audio_parameters)
    : audio_forwarder_(context),
      audio_parameters_(audio_parameters),
      main_thread_task_runner_(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      weak_handle_(MakeCrossThreadWeakHandle(this)) {
  audio_forwarder_.Bind(std::move(audio_forwarder), main_thread_task_runner_);
}

void SpeechRecognitionMediaStreamAudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &SpeechRecognitionMediaStreamAudioSink::SendAudio,
          MakeUnwrappingCrossThreadWeakHandle(weak_handle_),
          ConvertToAudioDataS16(audio_bus, audio_parameters_.sample_rate(),
                                audio_parameters_.channel_layout())));
}

void SpeechRecognitionMediaStreamAudioSink::OnSetFormat(
    const media::AudioParameters& audio_parameters) {
  audio_parameters_ = audio_parameters;
}

void SpeechRecognitionMediaStreamAudioSink::Trace(Visitor* visitor) const {
  visitor->Trace(audio_forwarder_);
}

void SpeechRecognitionMediaStreamAudioSink::SendAudio(
    media::mojom::blink::AudioDataS16Ptr audio_data) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  audio_forwarder_->AddAudioFromRenderer(std::move(audio_data));
}

media::mojom::blink::AudioDataS16Ptr
SpeechRecognitionMediaStreamAudioSink::ConvertToAudioDataS16(
    const media::AudioBus& audio_bus,
    int sample_rate,
    media::ChannelLayout channel_layout) {
  auto signed_buffer = media::mojom::blink::AudioDataS16::New();
  signed_buffer->channel_count = audio_bus.channels();
  signed_buffer->frame_count = audio_bus.frames();
  signed_buffer->sample_rate = sample_rate;

  // Mix the channels into a monaural channel before converting it if necessary.
  if (audio_bus.channels() > 1) {
    signed_buffer->channel_count = 1;

    ResetChannelMixerIfNeeded(audio_bus.frames(), channel_layout,
                              audio_bus.channels());
    signed_buffer->data.resize(audio_bus.frames());

    channel_mixer_->Transform(&audio_bus, monaural_audio_bus_.get());
    monaural_audio_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
        monaural_audio_bus_->frames(), &signed_buffer->data[0]);

    return signed_buffer;
  }

  signed_buffer->data.resize(audio_bus.frames() * audio_bus.channels());
  audio_bus.ToInterleaved<media::SignedInt16SampleTypeTraits>(
      audio_bus.frames(), &signed_buffer->data[0]);

  return signed_buffer;
}

void SpeechRecognitionMediaStreamAudioSink::ResetChannelMixerIfNeeded(
    int frame_count,
    media::ChannelLayout channel_layout,
    int channel_count) {
  if (!monaural_audio_bus_ || frame_count != monaural_audio_bus_->frames()) {
    monaural_audio_bus_ = media::AudioBus::Create(1 /*channels*/, frame_count);
  }

  if (channel_layout != channel_layout_ || channel_count != channel_count_) {
    channel_layout_ = channel_layout;
    channel_count_ = channel_count;
    channel_mixer_ = std::make_unique<media::ChannelMixer>(
        channel_layout, channel_count, media::CHANNEL_LAYOUT_MONO,
        1 /*output_channels*/);
  }
}

}  // namespace blink
