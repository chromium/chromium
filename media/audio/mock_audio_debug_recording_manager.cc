// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_debug_recording_manager.h"

#include <utility>

namespace media {

MockAudioDebugRecordingManager::MockAudioDebugRecordingManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : AudioDebugRecordingManager(std::move(task_runner)) {}

MockAudioDebugRecordingManager::~MockAudioDebugRecordingManager() = default;

}  // namespace media
