// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/audio_debug_file_writer.h"

namespace media {

AudioDebugRecordingHelper::AudioDebugRecordingHelper(
    const AudioParameters& params,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure on_destruction_closure)
    : params_(params),
      recording_enabled_(0),
      task_runner_(std::move(task_runner)),
      on_destruction_closure_(std::move(on_destruction_closure)) {}

AudioDebugRecordingHelper::~AudioDebugRecordingHelper() {
  if (on_destruction_closure_)
    std::move(on_destruction_closure_).Run();
}

void AudioDebugRecordingHelper::EnableDebugRecording(
    AudioDebugRecordingStreamType stream_type,
    uint32_t id,
    CreateWavFileCallback create_file_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!debug_writer_);

  debug_writer_ = CreateAudioDebugFileWriter(params_);
  std::move(create_file_callback)
      .Run(stream_type, id,
           base::BindOnce(&AudioDebugRecordingHelper::StartDebugRecordingToFile,
                          weak_factory_.GetWeakPtr()));
}

void AudioDebugRecordingHelper::StartDebugRecordingToFile(base::File file) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!file.IsValid()) {
    PLOG(ERROR) << "Invalid debug recording file, error="
                << file.error_details();
    debug_writer_.reset();
    return;
  }

  debug_writer_->Start(std::move(file));

  base::subtle::NoBarrier_Store(&recording_enabled_, 1);
}

void AudioDebugRecordingHelper::DisableDebugRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_Store(&recording_enabled_, 0);

  if (debug_writer_) {
    debug_writer_->Stop();
    debug_writer_.reset();
  }
}

void AudioDebugRecordingHelper::OnData(const AudioBus* source) {
  // Check if debug recording is enabled to avoid an unecessary copy and thread
  // jump if not. Recording can be disabled between the atomic Load() here and
  // DoWrite(), but it's fine with a single unnecessary copy+jump at disable
  // time. We use an atomic operation for accessing the flag on different
  // threads. No memory barrier is needed for the same reason; a race is no
  // problem at enable and disable time. Missing one buffer of data doesn't
  // matter.
  base::subtle::Atomic32 recording_enabled =
      base::subtle::NoBarrier_Load(&recording_enabled_);
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

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioDebugRecordingHelper::DoWrite,
                     weak_factory_.GetWeakPtr(), std::move(audio_bus_copy)));
}

void AudioDebugRecordingHelper::DoWrite(std::unique_ptr<media::AudioBus> data) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (debug_writer_)
    debug_writer_->Write(std::move(data));
}

std::unique_ptr<AudioDebugFileWriter>
AudioDebugRecordingHelper::CreateAudioDebugFileWriter(
    const AudioParameters& params) {
  return std::make_unique<AudioDebugFileWriter>(params);
}

}  // namespace media
