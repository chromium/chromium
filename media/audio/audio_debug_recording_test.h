// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_
#define MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_

#include <memory>

#include "base/test/task_environment.h"
#include "media/base/media_export.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockAudioDebugRecordingManager;
class MockAudioManager;

// Base test class for media/audio/ and services/audio/ debug recording test
// classes.
class AudioDebugRecordingTest : public testing::Test {
 public:
  AudioDebugRecordingTest();
  ~AudioDebugRecordingTest() override;

 protected:
  void CreateAudioManager();
  void ShutdownAudioManager();
  void InitializeAudioDebugRecordingManager();

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAudioManager> mock_audio_manager_;
  MockAudioDebugRecordingManager* mock_debug_recording_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingTest);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_TEST_H_
