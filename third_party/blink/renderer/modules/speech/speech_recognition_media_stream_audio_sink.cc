// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_media_stream_audio_sink.h"

#include <memory>

#include "base/time/time.h"
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
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace {
// Allocate 500ms worth of audio buffers. Audio is received on a real-time
// thread and is posted to the main thread, which has a bit lower priority and
// may also be blocked for long intervals due to garbage collection, for
// example. As soon as the pool reaches maximum capacity, it will fall back to
// allocating new buffers on the real-time thread until the main thread cathces
// up and processes the whole pool.
constexpr base::TimeDelta kAudioBusPoolDuration = base::Milliseconds(500);
}  // namespace

namespace blink {

SpeechRecognitionMediaStreamAudioSink::SpeechRecognitionMediaStreamAudioSink(
    ExecutionContext* context,
    StartRecognitionCallback start_recognition_callback)
    : audio_forwarder_(context),
      start_recognition_callback_(std::move(start_recognition_callback)),
      main_thread_task_runner_(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      weak_handle_(MakeCrossThreadWeakHandle(this)) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
}

void SpeechRecognitionMediaStreamAudioSink::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  CHECK(audio_bus_pool_);
  std::unique_ptr<media::AudioBus> audio_bus_copy =
      audio_bus_pool_->GetAudioBus();
  CHECK_EQ(audio_bus.channels(), audio_bus_copy->channels());
  CHECK_EQ(audio_bus.frames(), audio_bus_copy->frames());
  audio_bus.CopyTo(audio_bus_copy.get());

  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &SpeechRecognitionMediaStreamAudioSink::SendAudio,
          MakeUnwrappingCrossThreadWeakHandle(weak_handle_),
          std::move(audio_bus_copy),
          CrossThreadUnretained(
              audio_bus_pool_
                  .get())));  // Unretained is safe here because the audio bus
                              // pool is deleted on the main thread.
}

// This is always called at least once before OnData(), and on the same thread.
void SpeechRecognitionMediaStreamAudioSink::OnSetFormat(
    const media::AudioParameters& audio_parameters) {
  CHECK(audio_parameters.IsValid());

  // Reconfigure and start recognition on the main thread. Also, pass the old
  // audio bus pool to the main thread for deletion to avoid a race condition
  // because the threads are re-added to the pool on the main thread.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&SpeechRecognitionMediaStreamAudioSink::
                              ReconfigureAndMaybeStartRecognitionOnMainThread,
                          MakeUnwrappingCrossThreadWeakHandle(weak_handle_),
                          audio_parameters, std::move(audio_bus_pool_)));

  // Initialize the audio bus pool on the real-time thread so that it's
  // immediately available in `OnData()`.
  int number_of_audio_buses =
      std::ceil(kAudioBusPoolDuration / audio_parameters.GetBufferDuration());
  audio_bus_pool_ = std::make_unique<media::AudioBusPoolImpl>(
      audio_parameters, number_of_audio_buses, number_of_audio_buses);
}

void SpeechRecognitionMediaStreamAudioSink::Trace(Visitor* visitor) const {
  visitor->Trace(audio_forwarder_);
}

void SpeechRecognitionMediaStreamAudioSink::
    ReconfigureAndMaybeStartRecognitionOnMainThread(
        const media::AudioParameters& audio_parameters,
        std::unique_ptr<media::AudioBusPoolImpl> old_audio_bus_pool) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  audio_parameters_ = audio_parameters;
  if (start_recognition_callback_) {
    std::move(start_recognition_callback_)
        .Run(audio_parameters_, audio_forwarder_.BindNewPipeAndPassReceiver(
                                    main_thread_task_runner_));
  }

  // Delete the old audio bus pool on the main thread as it goes out of scope.
}

void SpeechRecognitionMediaStreamAudioSink::SendAudio(
    std::unique_ptr<media::AudioBus> audio_data,
    media::AudioBusPoolImpl* audio_bus_pool) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  audio_forwarder_->AddAudioFromRenderer(
      ConvertToAudioDataS16(*audio_data.get(), audio_parameters_.sample_rate(),
                            audio_parameters_.channel_layout()));

  audio_bus_pool->InsertAudioBus(std::move(audio_data));
}

media::mojom::blink::AudioDataS16Ptr
SpeechRecognitionMediaStreamAudioSink::ConvertToAudioDataS16(
    const media::AudioBus& audio_bus,
    int sample_rate,
    media::ChannelLayout channel_layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

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
  } else {
    signed_buffer->data.resize(audio_bus.frames() * audio_bus.channels());
    audio_bus.ToInterleaved<media::SignedInt16SampleTypeTraits>(
        audio_bus.frames(), &signed_buffer->data[0]);
  }

  return signed_buffer;
}

void SpeechRecognitionMediaStreamAudioSink::ResetChannelMixerIfNeeded(
    int frame_count,
    media::ChannelLayout channel_layout,
    int channel_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

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
