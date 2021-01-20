// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/audio/android/opensles_util.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerAndroid;

// Implements PCM audio input support for Android using the OpenSLES API.
// This class is created and lives on the Audio Manager thread but recorded
// audio buffers are delivered on an internal OpenSLES audio thread. All public
// methods should be called on the Audio Manager thread.
class OpenSLESInputStream : public AudioInputStream {
 public:
  static const int kMaxNumOfBuffersInQueue = 2;

  OpenSLESInputStream(AudioManagerAndroid* manager,
                      const AudioParameters& params);

  ~OpenSLESInputStream() override;

  // Implementation of AudioInputStream.
  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  bool CreateRecorder();

  // Called from OpenSLES specific audio worker thread.
  static void SimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf buffer_queue,
      void* instance);

  // Called from OpenSLES specific audio worker thread.
  void ReadBufferQueue();

  // Called in Open();
  void SetupAudioBuffer();

  // Called in Close();
  void ReleaseAudioBuffer();

  // If OpenSLES reports an error this function handles it and passes it to
  // the attached AudioInputCallback::OnError().
  void HandleError(SLresult error);

  base::ThreadChecker thread_checker_;

  // Protects |callback_|, |active_buffer_index_|, |audio_data_|,
  // |buffer_size_bytes_| and |simple_buffer_queue_|.
  base::Lock lock_;

  AudioManagerAndroid* audio_manager_;

  AudioInputCallback* callback_;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf recorder_object_;
  media::ScopedSLObjectItf engine_object_;

  SLRecordItf recorder_;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  SLDataFormat_PCM format_;

  // Audio buffers that are allocated in the constructor based on
  // info from audio parameters.
  uint8_t* audio_data_[kMaxNumOfBuffersInQueue];

  int active_buffer_index_;
  int buffer_size_bytes_;

  bool started_;

  base::TimeDelta hardware_delay_;

  std::unique_ptr<media::AudioBus> audio_bus_;

  // Set to true at construction if user wants to disable all audio effects.
  const bool no_effects_ = false;

  DISALLOW_COPY_AND_ASSIGN(OpenSLESInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_INPUT_H_
