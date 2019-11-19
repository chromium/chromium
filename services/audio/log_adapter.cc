// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/log_adapter.h"

#include <utility>

namespace audio {

LogAdapter::LogAdapter(mojo::PendingRemote<media::mojom::AudioLog> audio_log)
    : audio_log_(std::move(audio_log)) {}

LogAdapter::~LogAdapter() = default;

void LogAdapter::OnCreated(const media::AudioParameters& params,
                           const std::string& device_id) {
  audio_log_->OnCreated(params, device_id);
}

void LogAdapter::OnStarted() {
  audio_log_->OnStarted();
}

void LogAdapter::OnStopped() {
  audio_log_->OnStopped();
}

void LogAdapter::OnClosed() {
  audio_log_->OnClosed();
}

void LogAdapter::OnError() {
  audio_log_->OnError();
}

void LogAdapter::OnSetVolume(double volume) {
  audio_log_->OnSetVolume(volume);
}

void LogAdapter::OnProcessingStateChanged(const std::string& message) {
  audio_log_->OnProcessingStateChanged(message);
}

void LogAdapter::OnLogMessage(const std::string& message) {
  audio_log_->OnLogMessage(message);
}

}  // namespace audio
