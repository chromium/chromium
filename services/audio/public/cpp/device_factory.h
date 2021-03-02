// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_

#include <memory>
#include <string>

#include "media/audio/audio_input_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace audio {

using DeadStreamDetection = media::AudioInputDevice::DeadStreamDetection;

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<mojom::StreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream);

scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<mojom::StreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream,
    mojo::PendingRemote<media::mojom::AudioLog>);

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_
