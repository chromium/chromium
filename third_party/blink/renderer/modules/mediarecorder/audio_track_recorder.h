// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace blink {

class AudioTrackEncoder;
class MediaStreamComponent;
class NonMainThread;

// AudioTrackRecorder is a MediaStreamAudioSink that encodes the audio buses
// received from a Stream Audio Track. The class is constructed on a
// single thread (the main Render thread) but can receive MediaStreamAudioSink-
// related calls on a different "live audio" thread (referred to internally as
// the "capture thread"). It owns an internal thread to use for encoding, on
// which lives an AudioTrackEncoder with its own threading subtleties, see the
// implementation file.
class MODULES_EXPORT AudioTrackRecorder
    : public TrackRecorder<WebMediaStreamAudioSink> {
 public:
  enum class CodecId {
    // Do not change the order of codecs. Add new ones right before kLast.
    kOpus,
    kPcm,  // 32-bit little-endian float.
    kAac,
    kLast
  };

  enum class BitrateMode { kConstant, kVariable };

  using OnEncodedAudioCB =
      base::RepeatingCallback<void(const media::AudioParameters& params,
                                   std::string encoded_data,
                                   base::TimeTicks capture_time)>;

  static CodecId GetPreferredCodecId();

  AudioTrackRecorder(CodecId codec,
                     MediaStreamComponent* track,
                     OnEncodedAudioCB on_encoded_audio_cb,
                     base::OnceClosure on_track_source_ended_cb,
                     uint32_t bits_per_second,
                     BitrateMode bitrate_mode,
                     std::unique_ptr<NonMainThread> encoder_thread =
                         NonMainThread::CreateThread(ThreadCreationParams(
                             ThreadType::kAudioEncoderThread)));

  AudioTrackRecorder(const AudioTrackRecorder&) = delete;
  AudioTrackRecorder& operator=(const AudioTrackRecorder&) = delete;

  ~AudioTrackRecorder() override;

  // Implement MediaStreamAudioSink.
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks capture_time) override;

  void Pause();
  void Resume();

 private:
  // Creates an audio encoder from |codec|. Returns nullptr if the codec is
  // invalid.
  static scoped_refptr<AudioTrackEncoder> CreateAudioEncoder(
      CodecId codec,
      OnEncodedAudioCB on_encoded_audio_cb,
      uint32_t bits_per_second,
      BitrateMode bitrate_mode);

  void ConnectToTrack();
  void DisconnectFromTrack();
  void ShutdownEncoder();

  void Prefinalize();

  // We need to hold on to the Blink track to remove ourselves on destruction.
  Persistent<MediaStreamComponent> track_;

  // Thin wrapper around the chosen encoder.
  // |encoder_| should be initialized before |encoder_thread_| such that
  // |encoder_thread_| is destructed first. This, combined with all
  // AudioTrackEncoder work (aside from construction and destruction) happening
  // on |encoder_thread_|, should allow us to be sure that all AudioTrackEncoder
  // work is done by the time we destroy it on ATR's thread.
  const scoped_refptr<AudioTrackEncoder> encoder_;

  // The thread on which |encoder_| works.
  std::unique_ptr<NonMainThread> encoder_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;

  // Number of frames per chunked buffer passed to the encoder.
  int frames_per_chunk_ = 0;

  // Integer used for checking OnSetFormat/OnData against races. We can use
  // neither ThreadChecker (due to SilentSinkSuspender), nor SequenceChecker.
  // See crbug.com/1377367.
#if DCHECK_IS_ON()
  std::atomic<int> race_checker_{0};
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_
