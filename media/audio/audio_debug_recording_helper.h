// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_

#include <memory>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace base {
class File;
class SingleThreadTaskRunner;
}

namespace media {

class AudioBus;

enum class AudioDebugRecordingStreamType { kInput = 0, kOutput = 1 };

// Interface for feeding data to a recorder.
class AudioDebugRecorder {
 public:
  virtual ~AudioDebugRecorder() {}

  // If debug recording is enabled, copies audio data and makes sure it's
  // written on the right thread. Otherwise ignores the data. Can be called on
  // any thread.
  virtual void OnData(const AudioBus* source) = 0;
};

// A helper class for those who want to use AudioDebugFileWriter. It handles
// copying AudioBus data, thread jump (OnData() can be called on any
// thread), and creating and deleting the AudioDebugFileWriter at enable and
// disable. All functions except OnData() must be called on the thread
// |task_runner| belongs to.
// TODO(grunell): When input debug recording is moved to AudioManager, it should
// be possible to merge this class into AudioDebugFileWriter. One thread jump
// could be skipped then. Currently we have
// soundcard thread -> control thread -> file thread,
// and with the merge we should be able to do
// soundcard thread -> file thread.
class MEDIA_EXPORT AudioDebugRecordingHelper : public AudioDebugRecorder {
 public:
  using CreateWavFileCallback = base::OnceCallback<void(
      AudioDebugRecordingStreamType stream_type,
      uint32_t id,
      base::OnceCallback<void(base::File)> reply_callback)>;

  AudioDebugRecordingHelper(
      const AudioParameters& params,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure on_destruction_closure);
  ~AudioDebugRecordingHelper() override;

  // Enable debug recording. Creates |debug_writer_| and runs
  // |create_file_callback| to create debug recording file.
  virtual void EnableDebugRecording(AudioDebugRecordingStreamType stream_type,
                                    uint32_t id,
                                    CreateWavFileCallback create_file_callback);

  // Disable debug recording. Destroys |debug_writer_|.
  virtual void DisableDebugRecording();

  // AudioDebugRecorder implementation. Can be called on any thread.
  void OnData(const AudioBus* source) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioDebugRecordingHelperTest, EnableDisable);
  FRIEND_TEST_ALL_PREFIXES(AudioDebugRecordingHelperTest, OnData);

  // Writes debug data to |debug_writer_|.
  void DoWrite(std::unique_ptr<media::AudioBus> data);

  // Creates an AudioDebugFileWriter. Overridden by test.
  virtual std::unique_ptr<AudioDebugFileWriter> CreateAudioDebugFileWriter(
      const AudioParameters& params);

  // Passed to |create_file_callback| in EnableDebugRecording, to be called
  // after debug recording file was created.
  void StartDebugRecordingToFile(base::File file);

  const AudioParameters params_;
  std::unique_ptr<AudioDebugFileWriter> debug_writer_;

  // Used as a flag to indicate if recording is enabled, accessed on different
  // threads.
  base::subtle::Atomic32 recording_enabled_;

  // The task runner for accessing |debug_writer_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Runs in destructor if set.
  base::OnceClosure on_destruction_closure_;

  base::WeakPtrFactory<AudioDebugRecordingHelper> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingHelper);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_
