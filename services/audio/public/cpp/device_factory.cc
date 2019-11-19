// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/device_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "services/audio/public/cpp/input_ipc.h"
#include "services/audio/public/mojom/constants.mojom.h"

namespace audio {

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    std::unique_ptr<service_manager::Connector> connector,
    const std::string& device_id,
    mojo::PendingRemote<media::mojom::AudioLog> log) {
  DCHECK(connector);
  mojo::PendingRemote<mojom::StreamFactory> stream_factory;
  connector->Connect(audio::mojom::kServiceName,
                     stream_factory.InitWithNewPipeAndPassReceiver());

  std::unique_ptr<media::AudioInputIPC> ipc = std::make_unique<InputIPC>(
      std::move(stream_factory), device_id, std::move(log));

  return base::MakeRefCounted<media::AudioInputDevice>(
      std::move(ipc), media::AudioInputDevice::Purpose::kUserInput);
}

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<mojom::StreamFactory> stream_factory,
    const std::string& device_id) {
  std::unique_ptr<media::AudioInputIPC> ipc = std::make_unique<InputIPC>(
      std::move(stream_factory), device_id, mojo::NullRemote());

  return base::MakeRefCounted<media::AudioInputDevice>(
      std::move(ipc), media::AudioInputDevice::Purpose::kUserInput);
}

}  // namespace audio
