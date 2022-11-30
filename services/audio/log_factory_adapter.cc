// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/log_factory_adapter.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/log_adapter.h"

namespace audio {

const int kMaxPendingLogRequests = 500;

struct LogFactoryAdapter::PendingLogRequest {
  PendingLogRequest(media::mojom::AudioLogComponent component,
                    int component_id,
                    mojo::PendingReceiver<media::mojom::AudioLog> receiver)
      : component(component),
        component_id(component_id),
        receiver(std::move(receiver)) {}
  PendingLogRequest(PendingLogRequest&& other) = default;
  PendingLogRequest& operator=(PendingLogRequest&& other) = default;
  ~PendingLogRequest() = default;

  media::mojom::AudioLogComponent component;
  int component_id;
  mojo::PendingReceiver<media::mojom::AudioLog> receiver;
};

LogFactoryAdapter::LogFactoryAdapter() = default;
LogFactoryAdapter::~LogFactoryAdapter() = default;

void LogFactoryAdapter::SetLogFactory(
    mojo::PendingRemote<media::mojom::AudioLogFactory> log_factory) {
  if (log_factory_) {
    LOG(WARNING) << "Attempting to set log factory more than once. Ignoring "
                    "request.";
    return;
  }

  log_factory_.Bind(std::move(log_factory));
  while (!pending_requests_.empty()) {
    auto& front = pending_requests_.front();
    log_factory_->CreateAudioLog(front.component, front.component_id,
                                 std::move(front.receiver));
    pending_requests_.pop();
  }
}

std::unique_ptr<media::AudioLog> LogFactoryAdapter::CreateAudioLog(
    AudioComponent component,
    int component_id) {
  mojo::PendingRemote<media::mojom::AudioLog> remote_audio_log;
  auto audio_log_receiver = remote_audio_log.InitWithNewPipeAndPassReceiver();
  media::mojom::AudioLogComponent mojo_component =
      static_cast<media::mojom::AudioLogComponent>(component);
  if (log_factory_) {
    log_factory_->CreateAudioLog(mojo_component, component_id,
                                 std::move(audio_log_receiver));
  } else if (pending_requests_.size() >= kMaxPendingLogRequests) {
    LOG(WARNING) << "Maximum number of queued log requests exceeded. Fulfilling"
                    " request with fake log.";
    return fake_log_factory_.CreateAudioLog(component, component_id);
  } else {
    pending_requests_.emplace(mojo_component, component_id,
                              std::move(audio_log_receiver));
  }
  return std::make_unique<LogAdapter>(std::move(remote_audio_log));
}

}  // namespace audio
