// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOG_FACTORY_MANAGER_H_
#define SERVICES_AUDIO_LOG_FACTORY_MANAGER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/audio/log_factory_adapter.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"

namespace media {
class AudioLogFactory;
}

namespace audio {

// This class is used to provide the LogFactoryManager interface. It will
// typically be instantiated when needed and remain for the lifetime of the
// service.
class LogFactoryManager final : public mojom::LogFactoryManager {
 public:
  LogFactoryManager();

  LogFactoryManager(const LogFactoryManager&) = delete;
  LogFactoryManager& operator=(const LogFactoryManager&) = delete;

  ~LogFactoryManager() final;

  void Bind(mojo::PendingReceiver<mojom::LogFactoryManager> receiver);

  // LogFactoryManager implementation.
  void SetLogFactory(
      mojo::PendingRemote<media::mojom::AudioLogFactory> log_factory) final;
  media::AudioLogFactory* GetLogFactory();

 private:
  mojo::ReceiverSet<mojom::LogFactoryManager> receivers_;
  LogFactoryAdapter log_factory_adapter_;
  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOG_FACTORY_MANAGER_H_
