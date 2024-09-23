// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/threading/thread_checker.h"

namespace media {

namespace {
// Running id recording sources.
uint32_t g_next_stream_id = 1;
}

AudioDebugRecordingManager::AudioDebugRecordingManager() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AudioDebugRecordingManager::~AudioDebugRecordingManager() = default;

void AudioDebugRecordingManager::EnableDebugRecording(
    CreateWavFileCallback create_file_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!create_file_callback.is_null());
  create_file_callback_ = std::move(create_file_callback);

  for (const auto& it : debug_recording_helpers_) {
    uint32_t id = it.first;
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    AudioDebugRecordingStreamType stream_type = it.second.second;
    recording_helper->EnableDebugRecording(stream_type, id,
                                           create_file_callback_);
  }
}

void AudioDebugRecordingManager::DisableDebugRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!create_file_callback_.is_null());
  for (const auto& it : debug_recording_helpers_) {
    AudioDebugRecordingHelper* recording_helper = it.second.first;
    recording_helper->DisableDebugRecording();
  }
  create_file_callback_.Reset();
}

std::unique_ptr<AudioDebugRecorder>
AudioDebugRecordingManager::RegisterDebugRecordingSource(
    AudioDebugRecordingStreamType stream_type,
    const AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const uint32_t id = g_next_stream_id++;

  // Normally, the manager will outlive the one who registers and owns the
  // returned recorder. But to not require this we use a weak pointer.
  std::unique_ptr<AudioDebugRecordingHelper> recording_helper =
      CreateAudioDebugRecordingHelper(
          params,
          base::BindOnce(
              &AudioDebugRecordingManager::UnregisterDebugRecordingSource,
              weak_factory_.GetWeakPtr(), id));

  if (IsDebugRecordingEnabled()) {
    recording_helper->EnableDebugRecording(stream_type, id,
                                           create_file_callback_);
  }

  debug_recording_helpers_[id] =
      std::make_pair(recording_helper.get(), stream_type);

  return base::WrapUnique<AudioDebugRecorder>(recording_helper.release());
}

void AudioDebugRecordingManager::UnregisterDebugRecordingSource(uint32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = debug_recording_helpers_.find(id);
  CHECK(it != debug_recording_helpers_.end(), base::NotFatalUntil::M130);
  debug_recording_helpers_.erase(id);
}

std::unique_ptr<AudioDebugRecordingHelper>
AudioDebugRecordingManager::CreateAudioDebugRecordingHelper(
    const AudioParameters& params,
    base::OnceClosure on_destruction_closure) {
  return std::make_unique<AudioDebugRecordingHelper>(
      params, std::move(on_destruction_closure));
}

bool AudioDebugRecordingManager::IsDebugRecordingEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !create_file_callback_.is_null();
}

}  // namespace media
