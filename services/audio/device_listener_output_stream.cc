// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_listener_output_stream.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace audio {

DeviceListenerOutputStream::DeviceListenerOutputStream(
    media::AudioManager* audio_manager,
    media::AudioOutputStream* wrapped_stream,
    base::OnceClosure on_device_change_callback)
    : audio_manager_(audio_manager),
      stream_(wrapped_stream),
      on_device_change_callback_(std::move(on_device_change_callback)),
      task_runner_(audio_manager->GetTaskRunner()) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream_);
  audio_manager_->AddOutputDeviceChangeListener(this);
}

DeviceListenerOutputStream::~DeviceListenerOutputStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  audio_manager_->RemoveOutputDeviceChangeListener(this);
}

bool DeviceListenerOutputStream::Open() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(on_device_change_callback_);
  return stream_->Open();
}

void DeviceListenerOutputStream::Start(
    media::AudioOutputStream::AudioSourceCallback* source_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!source_callback_);
  DCHECK(source_callback);
  DCHECK(on_device_change_callback_);
  source_callback_ = source_callback;
  stream_->Start(this);
}

void DeviceListenerOutputStream::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  stream_->Stop();
  source_callback_ = nullptr;
}

void DeviceListenerOutputStream::SetVolume(double volume) {
  stream_->SetVolume(volume);
}

void DeviceListenerOutputStream::GetVolume(double* volume) {
  stream_->GetVolume(volume);
}

void DeviceListenerOutputStream::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!source_callback_);
  // ExtractAsDangling clears the underlying pointer and returns another raw_ptr
  // instance that is allowed to dangle.
  stream_.ExtractAsDangling()->Close();
  // To match a typical AudioOutputStream usage pattern.
  delete this;
}

void DeviceListenerOutputStream::Flush() {
  stream_->Flush();
}

void DeviceListenerOutputStream::OnDeviceChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::move(on_device_change_callback_).Run();
  // Close() must have been called and |this| deleted at this point.
}

int DeviceListenerOutputStream::OnMoreData(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const media::AudioGlitchInfo& glitch_info,

    media::AudioBus* dest) {
  return source_callback_->OnMoreData(delay, delay_timestamp, glitch_info,
                                      dest);
}

int DeviceListenerOutputStream::OnMoreData(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const media::AudioGlitchInfo& glitch_info,
    media::AudioBus* dest,
    bool is_mixing) {
  return source_callback_->OnMoreData(delay, delay_timestamp, glitch_info, dest,
                                      is_mixing);
}

void DeviceListenerOutputStream::OnError(ErrorType type) {
  if (type == ErrorType::kDeviceChange) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeviceListenerOutputStream::OnDeviceChange,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // Handles errors on the audio manager thread. We defer errors for one
  // second in case they are the result of a device change; delay chosen to
  // exceed duration of device changes which take a few hundred milliseconds.
  // |this| will be deleted after the client processes the device change, that
  // way the posted callback will be cancelled.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceListenerOutputStream::ReportError,
                     weak_factory_.GetWeakPtr(), type),
      base::Seconds(1));
}

void DeviceListenerOutputStream::ReportError(ErrorType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(type, ErrorType::kDeviceChange);
  if (source_callback_)
    source_callback_->OnError(type);
}

}  // namespace audio
