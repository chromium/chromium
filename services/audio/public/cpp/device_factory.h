// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "media/audio/audio_input_device.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace audio {

using DeadStreamDetection = media::AudioInputDevice::DeadStreamDetection;

COMPONENT_EXPORT(AUDIO_PUBLIC_CPP)
scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream);

COMPONENT_EXPORT(AUDIO_PUBLIC_CPP)
scoped_refptr<media::AudioCapturerSource> CreateInputDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    DeadStreamDetection detect_dead_stream,
    mojo::PendingRemote<media::mojom::AudioLog>);

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEVICE_FACTORY_H_
