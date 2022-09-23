// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
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
      file_writer_.reset();
    }
  }
}

void AudioDebugRecordingHelper::OnData(const AudioBus* source) {
  // Check if debug recording is enabled to avoid an unnecessary copy and thread
  // jump if not.
  bool recording_enabled = false;
  if (file_writer_lock_.Try()) {
    recording_enabled = static_cast<bool>(file_writer_);
    file_writer_lock_.Release();
  }
  if (!recording_enabled)
    return;

  // TODO(tommi): This is costly. AudioBus heap allocs and we create a new one
  // for every callback. We could instead have a pool of bus objects that get
  // returned to us somehow.
  // We should also avoid calling PostTask here since the implementation of the
  // debug writer will basically do a PostTask straight away anyway. Might
  // require some modifications to AudioDebugFileWriter though since there are
  // some threading concerns there and AudioDebugFileWriter's lifetime
  // guarantees need to be longer than that of associated active audio streams.
  std::unique_ptr<AudioBus> audio_bus_copy =
      AudioBus::Create(source->channels(), source->frames());
  source->CopyTo(audio_bus_copy.get());

  if (file_writer_lock_.Try()) {
    if (file_writer_) {
      file_writer_->Write(std::move(audio_bus_copy));
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

}  // namespace media
