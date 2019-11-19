// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOG_FACTORY_ADAPTER_H_
#define SERVICES_AUDIO_LOG_FACTORY_ADAPTER_H_

#include <memory>

#include "base/containers/queue.h"
#include "media/audio/audio_logging.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"

namespace media {
class AudioLogFactory;
}  // namespace media

namespace audio {

// This class allows setting a mojo audio log factory to create audio logs
// in the audio service. It also acts as a media::AudioLogFactory to interface
// with AudioManager.
class LogFactoryAdapter final : public media::AudioLogFactory {
 public:
  LogFactoryAdapter();
  ~LogFactoryAdapter() final;

  void SetLogFactory(
      mojo::PendingRemote<media::mojom::AudioLogFactory> log_factory);

  // media::AudioLogFactory implementation
  std::unique_ptr<media::AudioLog> CreateAudioLog(AudioComponent component,
                                                  int component_id) override;

 private:
  struct PendingLogRequest;

  mojo::Remote<media::mojom::AudioLogFactory> log_factory_;
  base::queue<PendingLogRequest> pending_requests_;
  media::FakeAudioLogFactory fake_log_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogFactoryAdapter);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOG_FACTORY_ADAPTER_H_
