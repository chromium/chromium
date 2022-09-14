// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AECDUMP_RECORDING_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AECDUMP_RECORDING_MANAGER_H_

#include "base/task/single_thread_task_runner.h"
#include "media/audio/aecdump_recording_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAecdumpRecordingManager : public AecdumpRecordingManager {
 public:
  explicit MockAecdumpRecordingManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  MockAecdumpRecordingManager(const MockAecdumpRecordingManager&) = delete;
  MockAecdumpRecordingManager& operator=(const MockAecdumpRecordingManager&) =
      delete;

  ~MockAecdumpRecordingManager() override;

  MOCK_METHOD1(RegisterAecdumpSource, void(AecdumpRecordingSource*));
  MOCK_METHOD1(DeregisterAecdumpSource, void(AecdumpRecordingSource*));

  MOCK_METHOD1(EnableDebugRecording, void(CreateFileCallback));
  MOCK_METHOD0(DisableDebugRecording, void());
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AECDUMP_RECORDING_MANAGER_H_
