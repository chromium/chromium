// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAudioDebugRecordingManager : public AudioDebugRecordingManager {
 public:
  MockAudioDebugRecordingManager();

  MockAudioDebugRecordingManager(const MockAudioDebugRecordingManager&) =
      delete;
  MockAudioDebugRecordingManager& operator=(
      const MockAudioDebugRecordingManager&) = delete;

  ~MockAudioDebugRecordingManager() override;

  MOCK_METHOD1(EnableDebugRecording,
               void(AudioDebugRecordingManager::CreateWavFileCallback
                        create_file_callback));
  MOCK_METHOD0(DisableDebugRecording, void());
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_
