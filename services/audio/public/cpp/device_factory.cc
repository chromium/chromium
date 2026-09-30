// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/device_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/threading/platform_thread.h"
#include "services/audio/public/cpp/input_ipc.h"

namespace audio {

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    std::optional<media::AudioProcessingSettings> processing_settings) {
  std::unique_ptr<media::AudioInputIPC> ipc = std::make_unique<InputIPC>(
      std::move(stream_factory), device_id, std::move(log),
      std::move(processing_settings));
  return base::MakeRefCounted<media::AudioInputDevice>(
      std::move(ipc), media::AudioInputDevice::Purpose::kUserInput,
      detect_dead_stream);
}

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream,
    std::optional<media::AudioProcessingSettings> processing_settings) {
  return CreateInputDevice(std::move(stream_factory), device_id,
                           detect_dead_stream, mojo::NullRemote(),
                           std::move(processing_settings));
}

}  // namespace audio
