// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_helper.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/base/audio_bus.h"

namespace media {

AudioDebugRecordingHelper::AudioDebugRecordingHelper(
    const AudioParameters& params,
    base::OnceClosure on_destruction_closure)
    : params_(params),
      file_writer_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      on_destruction_closure_(std::move(on_destruction_closure)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AudioDebugRecordingHelper::~AudioDebugRecordingHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_destruction_closure_)
    std::move(on_destruction_closure_).Run();
}

void AudioDebugRecordingHelper::EnableDebugRecording(
    AudioDebugRecordingStreamType stream_type,
    uint32_t id,
    CreateWavFileCallback create_file_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(create_file_callback)
      .Run(stream_type, id,
           base::BindOnce(&AudioDebugRecordingHelper::StartDebugRecordingToFile,
                          weak_factory_.GetWeakPtr()));
}

void AudioDebugRecordingHelper::StartDebugRecordingToFile(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  {
    base::AutoLock auto_lock(file_writer_lock_);

    if (!file.IsValid()) {
      PLOG(ERROR) << "Invalid debug recording file, error="
                  << file.error_details();
      file_writer_.reset();
      return;
    }

    file_writer_ = CreateAudioDebugFileWriter(params_, std::move(file));
  }
}

void AudioDebugRecordingHelper::DisableDebugRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  {
    base::AutoLock auto_lock(file_writer_lock_);
    if (file_writer_) {
      WillDestroyAudioDebugFileWriter();
      file_writer_.reset();
    }
  }
}

void AudioDebugRecordingHelper::OnData(const AudioBus* source) {
  if (file_writer_lock_.Try()) {
    if (file_writer_) {
      file_writer_->Write(*source);
    }
    file_writer_lock_.Release();
  }
}

AudioDebugFileWriter::Ptr AudioDebugRecordingHelper::CreateAudioDebugFileWriter(
    const AudioParameters& params,
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AudioDebugFileWriter::Create(params, std::move(file));
}

void AudioDebugRecordingHelper::WillDestroyAudioDebugFileWriter() {
  // No special action required.
}

}  // namespace media
