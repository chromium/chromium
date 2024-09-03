// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"
#include <memory>

#include "base/check_op.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_mojo_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_opus_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_pcm_encoder.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

#if BUILDFLAG(IS_WIN) || ((BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)) && \
                          BUILDFLAG(USE_PROPRIETARY_CODECS))
#define HAS_AAC_ENCODER 1
#endif

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

// Max size of buffers passed on to encoders.
const int kMaxChunkedBufferDurationMs = 60;

AudioTrackRecorder::CodecId AudioTrackRecorder::GetPreferredCodecId(
    MediaTrackContainerType type) {
  return CodecId::kOpus;
}

AudioTrackRecorder::AudioTrackRecorder(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    CodecId codec,
    MediaStreamComponent* track,
    WeakCell<CallbackInterface>* callback_interface,
    uint32_t bits_per_second,
    BitrateMode bitrate_mode,
    scoped_refptr<base::SequencedTaskRunner> encoder_task_runner)
    : TrackRecorder(base::BindPostTask(
          main_thread_task_runner,
          WTF::BindOnce(&CallbackInterface::OnSourceReadyStateChanged,
                        WrapPersistent(callback_interface)))),
      track_(track),
      encoder_task_runner_(std::move(encoder_task_runner)),
      encoder_(encoder_task_runner_,
               CreateAudioEncoder(
                   codec,
                   encoder_task_runner_,
                   base::BindPostTask(
                       main_thread_task_runner,
                       WTF::BindRepeating(&CallbackInterface::OnEncodedAudio,
                                          WrapPersistent(callback_interface))),
                   base::BindPostTask(
                       main_thread_task_runner,
                       WTF::BindOnce(&CallbackInterface::OnAudioEncodingError,
                                     WrapPersistent(callback_interface))),
                   bits_per_second,
                   bitrate_mode)),
      callback_interface_(callback_interface) {
  DCHECK(IsMainThread());
  DCHECK(track_);
  DCHECK(track_->GetSourceType() == MediaStreamSource::kTypeAudio);

  // Connect the source provider to the track as a sink.
  ConnectToTrack();
}

AudioTrackRecorder::~AudioTrackRecorder() {
  DCHECK(IsMainThread());
  DisconnectFromTrack();
}

// Creates an audio encoder from the codec. Returns nullptr if the codec is
// invalid.
std::unique_ptr<AudioTrackEncoder> AudioTrackRecorder::CreateAudioEncoder(
    CodecId codec,
    scoped_refptr<base::SequencedTaskRunner> encoder_task_runner,
    OnEncodedAudioCB on_encoded_audio_cb,
    OnEncodedAudioErrorCB on_encoded_audio_error_cb,
    uint32_t bits_per_second,
    BitrateMode bitrate_mode) {
  std::unique_ptr<AudioTrackEncoder> encoder;
  switch (codec) {
    case CodecId::kPcm:
      return std::make_unique<AudioTrackPcmEncoder>(
          std::move(on_encoded_audio_cb), std::move(on_encoded_audio_error_cb));
    case CodecId::kAac:
#if HAS_AAC_ENCODER
      return std::make_unique<AudioTrackMojoEncoder>(
          encoder_task_runner, codec, std::move(on_encoded_audio_cb),
          std::move(on_encoded_audio_error_cb), bits_per_second);
#endif
      NOTREACHED_IN_MIGRATION() << "AAC encoder is not supported.";
      return nullptr;
    case CodecId::kOpus:
    default:
      return std::make_unique<AudioTrackOpusEncoder>(
          std::move(on_encoded_audio_cb), std::move(on_encoded_audio_error_cb),
          bits_per_second, bitrate_mode == BitrateMode::kVariable);
  }
}

void AudioTrackRecorder::OnSetFormat(const media::AudioParameters& params) {
#if DCHECK_IS_ON()
  CHECK_EQ(race_checker_.fetch_add(1), 0) << __func__ << ": race detected.";
#endif
  int max_frames_per_chunk = params.sample_rate() *
                             kMaxChunkedBufferDurationMs /
                             base::Time::kMillisecondsPerSecond;

  frames_per_chunk_ =
      std::min(params.frames_per_buffer(), max_frames_per_chunk);

  encoder_.AsyncCall(&AudioTrackEncoder::OnSetFormat).WithArgs(params);
#if DCHECK_IS_ON()
  race_checker_.store(0);
#endif
}

void AudioTrackRecorder::OnData(const media::AudioBus& audio_bus,
                                base::TimeTicks capture_time) {
#if DCHECK_IS_ON()
  CHECK_EQ(race_checker_.fetch_add(1), 0) << __func__ << ": race detected.";
#endif
  DCHECK(!capture_time.is_null());
  DCHECK_GT(frames_per_chunk_, 0) << "OnSetFormat not called before OnData";

  for (int chunk_start = 0; chunk_start < audio_bus.frames();
       chunk_start += frames_per_chunk_) {
    std::unique_ptr<media::AudioBus> audio_data =
        media::AudioBus::Create(audio_bus.channels(), frames_per_chunk_);
    int chunk_size = chunk_start + frames_per_chunk_ >= audio_bus.frames()
                         ? audio_bus.frames() - chunk_start
                         : frames_per_chunk_;
    audio_bus.CopyPartialFramesTo(chunk_start, chunk_size, 0, audio_data.get());

    encoder_.AsyncCall(&AudioTrackEncoder::EncodeAudio)
        .WithArgs(std::move(audio_data), capture_time);
  }
#if DCHECK_IS_ON()
  race_checker_.store(0);
#endif
}

void AudioTrackRecorder::Pause() {
  DCHECK(IsMainThread());
  DCHECK(encoder_);
  encoder_.AsyncCall(&AudioTrackEncoder::set_paused).WithArgs(true);
}

void AudioTrackRecorder::Resume() {
  DCHECK(IsMainThread());
  DCHECK(encoder_);
  encoder_.AsyncCall(&AudioTrackEncoder::set_paused).WithArgs(false);
}

void AudioTrackRecorder::ConnectToTrack() {
  track_->AddSink(this);
}

void AudioTrackRecorder::DisconnectFromTrack() {
  auto* audio_track =
      static_cast<MediaStreamAudioTrack*>(track_->GetPlatformTrack());
  DCHECK(audio_track);
  audio_track->RemoveSink(this);
}

}  // namespace blink
