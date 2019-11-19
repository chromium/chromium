// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace blink {

class AudioTrackEncoder;
class MediaStreamComponent;
class Thread;

// AudioTrackRecorder is a MediaStreamAudioSink that encodes the audio buses
// received from a Stream Audio Track. The class is constructed on a
// single thread (the main Render thread) but can receive MediaStreamAudioSink-
// related calls on a different "live audio" thread (referred to internally as
// the "capture thread"). It owns an internal thread to use for encoding, on
// which lives an AudioTrackEncoder with its own threading subtleties, see the
// implementation file.
class MODULES_EXPORT AudioTrackRecorder
    : public GarbageCollected<AudioTrackRecorder>,
      public WebMediaStreamAudioSink {
  USING_PRE_FINALIZER(AudioTrackRecorder, Prefinalize);

 public:
  enum class CodecId {
    // Do not change the order of codecs. Add new ones right before LAST.
    OPUS,
    PCM,  // 32-bit little-endian float.
    LAST
  };

  using OnEncodedAudioCB =
      base::RepeatingCallback<void(const media::AudioParameters& params,
                                   std::string encoded_data,
                                   base::TimeTicks capture_time)>;

  static CodecId GetPreferredCodecId();

  AudioTrackRecorder(CodecId codec,
                     MediaStreamComponent* track,
                     OnEncodedAudioCB on_encoded_audio_cb,
                     int32_t bits_per_second);
  ~AudioTrackRecorder() override;

  // Implement MediaStreamAudioSink.
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks capture_time) override;

  void Pause();
  void Resume();

  void Trace(blink::Visitor*);

 private:
  // Creates an audio encoder from |codec|. Returns nullptr if the codec is
  // invalid.
  static scoped_refptr<AudioTrackEncoder> CreateAudioEncoder(
      CodecId codec,
      OnEncodedAudioCB on_encoded_audio_cb,
      int32_t bits_per_second);

  void ConnectToTrack();
  void DisconnectFromTrack();

  void Prefinalize();

  // Used to check that MediaStreamAudioSink's methods are called on the
  // capture audio thread.
  THREAD_CHECKER(capture_thread_checker_);

  // We need to hold on to the Blink track to remove ourselves on destruction.
  Member<MediaStreamComponent> track_;

  // Thin wrapper around OpusEncoder.
  // |encoder_| should be initialized before |encoder_thread_| such that
  // |encoder_thread_| is destructed first. This, combined with all
  // AudioTrackEncoder work (aside from construction and destruction) happening
  // on |encoder_thread_|, should allow us to be sure that all AudioTrackEncoder
  // work is done by the time we destroy it on ATR's thread.
  const scoped_refptr<AudioTrackEncoder> encoder_;

  // The thread on which |encoder_| works.
  std::unique_ptr<Thread> encoder_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioTrackRecorder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_RECORDER_H_
