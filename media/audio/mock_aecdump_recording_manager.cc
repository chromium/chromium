// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_aecdump_recording_manager.h"
#include "base/task/single_thread_task_runner.h"

#include <utility>

namespace media {

MockAecdumpRecordingManager::MockAecdumpRecordingManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : AecdumpRecordingManager(task_runner) {}

MockAecdumpRecordingManager::~MockAecdumpRecordingManager() = default;

}  // namespace media
