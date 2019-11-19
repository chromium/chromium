// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"

#include "base/macros.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_opus_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_pcm_encoder.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

// Note that this code follows the Chrome media convention of defining a "frame"
// as "one multi-channel sample" as opposed to another common definition meaning
// "a chunk of samples". Here this second definition of "frame" is called a
// "buffer"; so what might be called "frame duration" is instead "buffer
// duration", and so on.

namespace WTF {

template <>
struct CrossThreadCopier<media::AudioParameters> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = media::AudioParameters;
  static Type Copy(Type pointer) { return pointer; }
};

}  // namespace WTF

namespace blink {

AudioTrackRecorder::CodecId AudioTrackRecorder::GetPreferredCodecId() {
  return CodecId::OPUS;
}

AudioTrackRecorder::AudioTrackRecorder(CodecId codec,
                                       MediaStreamComponent* track,
                                       OnEncodedAudioCB on_encoded_audio_cb,
                                       int32_t bits_per_second)
    : track_(track),
      encoder_(CreateAudioEncoder(codec,
                                  std::move(on_encoded_audio_cb),
                                  bits_per_second)),
      encoder_thread_(Thread::CreateThread(
          ThreadCreationParams(ThreadType::kAudioEncoderThread))),
      encoder_task_runner_(encoder_thread_->GetTaskRunner()) {
  DCHECK(IsMainThread());
  DCHECK(track_);
  DCHECK(track_->Source()->GetType() == MediaStreamSource::kTypeAudio);

  // Connect the source provider to the track as a sink.
  ConnectToTrack();
}

AudioTrackRecorder::~AudioTrackRecorder() = default;

// Creates an audio encoder from the codec. Returns nullptr if the codec is
// invalid.
scoped_refptr<AudioTrackEncoder> AudioTrackRecorder::CreateAudioEncoder(
    CodecId codec,
    OnEncodedAudioCB on_encoded_audio_cb,
    int32_t bits_per_second) {
  if (codec == CodecId::PCM) {
    return base::MakeRefCounted<AudioTrackPcmEncoder>(
        media::BindToCurrentLoop(std::move(on_encoded_audio_cb)));
  }

  // All other paths will use the AudioTrackOpusEncoder.
  return base::MakeRefCounted<AudioTrackOpusEncoder>(
      media::BindToCurrentLoop(std::move(on_encoded_audio_cb)),
      bits_per_second);
}

void AudioTrackRecorder::OnSetFormat(const media::AudioParameters& params) {
  // If the source is restarted, might have changed to another capture thread.
  DETACH_FROM_THREAD(capture_thread_checker_);
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  PostCrossThreadTask(
      *encoder_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&AudioTrackEncoder::OnSetFormat, encoder_, params));
}

void AudioTrackRecorder::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  DCHECK(!capture_time.is_null());

  std::unique_ptr<media::AudioBus> audio_data =
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
  audio_bus.CopyTo(audio_data.get());

  PostCrossThreadTask(
      *encoder_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&AudioTrackEncoder::EncodeAudio, encoder_,
                          std::move(audio_data), capture_time));
}

void AudioTrackRecorder::Pause() {
  DCHECK(IsMainThread());
  DCHECK(encoder_);
  PostCrossThreadTask(
      *encoder_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&AudioTrackEncoder::set_paused, encoder_, true));
}

void AudioTrackRecorder::Resume() {
  DCHECK(IsMainThread());
  DCHECK(encoder_);
  PostCrossThreadTask(
      *encoder_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&AudioTrackEncoder::set_paused, encoder_, false));
}

void AudioTrackRecorder::ConnectToTrack() {
  auto* audio_track =
      static_cast<MediaStreamAudioTrack*>(track_->GetPlatformTrack());
  DCHECK(audio_track);
  audio_track->AddSink(this);
}

void AudioTrackRecorder::DisconnectFromTrack() {
  auto* audio_track =
      static_cast<MediaStreamAudioTrack*>(track_->GetPlatformTrack());
  DCHECK(audio_track);
  audio_track->RemoveSink(this);
}

void AudioTrackRecorder::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
}

void AudioTrackRecorder::Prefinalize() {
  // TODO(crbug.com/704136) : Remove this method when moving
  // MediaStreamAudioTrack to Oilpan's heap.
  DCHECK(IsMainThread());
  DisconnectFromTrack();
  track_ = nullptr;
}

}  // namespace blink
