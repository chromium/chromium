// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOG_ADAPTER_H_
#define SERVICES_AUDIO_LOG_ADAPTER_H_

#include <string>

#include "media/audio/audio_logging.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioParameters;
}

namespace audio {

// This class wraps a Remote<media::mojom::AudioLog> into a media::AudioLog.
class LogAdapter : public media::AudioLog {
 public:
  explicit LogAdapter(mojo::PendingRemote<media::mojom::AudioLog> audio_log);

  LogAdapter(const LogAdapter&) = delete;
  LogAdapter& operator=(const LogAdapter&) = delete;

  ~LogAdapter() override;

  // media::AudioLog implementation.
  void OnCreated(const media::AudioParameters& params,
                 const std::string& device_id) override;
  void OnStarted() override;
  void OnStopped() override;
  void OnClosed() override;
  void OnError() override;
  void OnSetVolume(double volume) override;
  void OnProcessingStateChanged(const std::string& message) override;
  void OnLogMessage(const std::string& message) override;

 private:
  mojo::Remote<media::mojom::AudioLog> audio_log_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOG_ADAPTER_H_
