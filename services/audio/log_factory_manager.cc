// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/log_factory_manager.h"

#include <utility>

namespace audio {

LogFactoryManager::LogFactoryManager() = default;

LogFactoryManager::~LogFactoryManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

void LogFactoryManager::Bind(
    mojo::PendingReceiver<mojom::LogFactoryManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  receivers_.Add(this, std::move(receiver));
}

void LogFactoryManager::SetLogFactory(
    mojo::PendingRemote<media::mojom::AudioLogFactory> log_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  log_factory_adapter_.SetLogFactory(std::move(log_factory));
}

media::AudioLogFactory* LogFactoryManager::GetLogFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return &log_factory_adapter_;
}

}  // namespace audio
