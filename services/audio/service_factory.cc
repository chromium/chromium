// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/owning_audio_manager_accessor.h"
#include "services/audio/service.h"

namespace audio {

std::unique_ptr<Service> CreateEmbeddedService(
    media::AudioManager* audio_manager,
    mojo::PendingReceiver<mojom::AudioService> receiver) {
  return std::make_unique<Service>(
      std::make_unique<InProcessAudioManagerAccessor>(audio_manager),
      /*enable_remote_client_support=*/false, std::move(receiver));
}

std::unique_ptr<Service> CreateStandaloneService(
    mojo::PendingReceiver<mojom::AudioService> receiver) {
  return std::make_unique<Service>(
      std::make_unique<audio::OwningAudioManagerAccessor>(
          base::BindOnce(&media::AudioManager::Create)),
      /*enable_remote_client_support=*/true, std::move(receiver));
}

}  // namespace audio
