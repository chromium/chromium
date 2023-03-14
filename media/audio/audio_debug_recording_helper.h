// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_

#include "base/atomicops.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace base {
class File;
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
// copying AudioBus data, thread jump (OnData() can be called on any thread),
// and creating and deleting the AudioDebugFileWriter at enable and disable. All
// public methods except OnData() must be called on the same sequence.
class MEDIA_EXPORT AudioDebugRecordingHelper : public AudioDebugRecorder {
 public:
  using CreateWavFileCallback = base::OnceCallback<void(
      AudioDebugRecordingStreamType stream_type,
      uint32_t id,
      base::OnceCallback<void(base::File)> reply_callback)>;

  AudioDebugRecordingHelper(const AudioParameters& params,
                            base::OnceClosure on_destruction_closure);

  AudioDebugRecordingHelper(const AudioDebugRecordingHelper&) = delete;
  AudioDebugRecordingHelper& operator=(const AudioDebugRecordingHelper&) =
      delete;

  ~AudioDebugRecordingHelper() override;

  // Enable debug recording. Runs |create_file_callback| synchronously to create
  // the debug recording file.
  virtual void EnableDebugRecording(AudioDebugRecordingStreamType stream_type,
                                    uint32_t id,
                                    CreateWavFileCallback create_file_callback);

  virtual void DisableDebugRecording();

  // AudioDebugRecorder implementation. Can be called on any thread.
  void OnData(const AudioBus* source) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioDebugRecordingHelperTest, EnableDisable);
  FRIEND_TEST_ALL_PREFIXES(AudioDebugRecordingHelperTest, OnData);

  // Creates an AudioDebugFileWriter. Overridden by test.
  virtual AudioDebugFileWriter::Ptr CreateAudioDebugFileWriter(
      const AudioParameters& params,
      base::File file);

  // Notifier for AudioDebugFileWriter destruction. Overridden by test.
  virtual void WillDestroyAudioDebugFileWriter();

  // Passed to |create_file_callback| in EnableDebugRecording, to be called
  // after debug recording file was created.
  void StartDebugRecordingToFile(base::File file);

  const AudioParameters params_;

  // Locks access to the |file_writer_|.
  base::Lock file_writer_lock_;

  AudioDebugFileWriter::Ptr file_writer_ GUARDED_BY(file_writer_lock_);

  // Runs in destructor if set.
  base::OnceClosure on_destruction_closure_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioDebugRecordingHelper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_HELPER_H_
