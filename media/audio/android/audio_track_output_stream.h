// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_TRACK_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_TRACK_OUTPUT_STREAM_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/time/tick_clock.h"
#include "media/audio/android/muteable_audio_output_stream.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerBase;

// A MuteableAudioOutputStream implementation based on the Android AudioTrack
// API.
class MEDIA_EXPORT AudioTrackOutputStream : public MuteableAudioOutputStream {
 public:
  AudioTrackOutputStream(AudioManagerBase* manager,
                         const AudioParameters& params);
  ~AudioTrackOutputStream() override;

  // AudioOutputStream implementation.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;
  void Flush() override;

  // MuteableAudioOutputStream implementation.
  void SetMute(bool muted) override;

  // AudioOutputStream::SourceCallback implementation methods called from Java.
  base::android::ScopedJavaLocalRef<jobject> OnMoreData(JNIEnv* env,
                                                        jobject obj,
                                                        jobject audio_data,
                                                        jlong delay);
  void OnError(JNIEnv* env, jobject obj);
  jlong GetAddress(JNIEnv* env, jobject obj, jobject byte_buffer);

 private:
  const AudioParameters params_;

  AudioManagerBase* audio_manager_;
  AudioSourceCallback* callback_ = nullptr;
  bool muted_ = false;
  double volume_ = 1.0;

  // Extra buffer for PCM format.
  std::unique_ptr<AudioBus> audio_bus_;

  const base::TickClock* tick_clock_;

  // Java AudioTrackOutputStream instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_output_stream_;

  DISALLOW_COPY_AND_ASSIGN(AudioTrackOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_TRACK_OUTPUT_STREAM_H_
