// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SYSTEM_LOOPBACK_LISTENER_H_
#define SERVICES_AUDIO_SYSTEM_LOOPBACK_LISTENER_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_manager.h"
#include "services/audio/device_output_listener.h"
#include "services/audio/reference_output.h"

namespace audio {

class SystemLoopbackListener : public DeviceOutputListener {
 public:
  class AudioCallback;

  explicit SystemLoopbackListener(media::AudioManager* audio_manager);
  SystemLoopbackListener(const SystemLoopbackListener&) = delete;
  SystemLoopbackListener& operator=(const SystemLoopbackListener&) = delete;
  ~SystemLoopbackListener() override;

  void StartListening(ReferenceOutput::Listener* listener,
                      const std::string& device_id) override;

  void StopListening(ReferenceOutput::Listener* listener) override;

 private:
  void EnsureLoopbackStreamStarted();
  void EnsureLoopbackStreamClosed();

  SEQUENCE_CHECKER(owning_sequence_);
  const raw_ptr<media::AudioManager> audio_manager_;
  raw_ptr<media::AudioInputStream> loopback_stream_ = nullptr;
  std::unique_ptr<media::AudioLog> audio_log_;
  // To differentiate the streams that SystemLoopbackListener creates from the
  // InputControllers, we start their ids at 1000000.
  // TODO(crbug.com/412581642): Remove this hack once the reference streams get
  // their own category.
  int next_loopback_stream_id_ = 1000000;
  std::unique_ptr<AudioCallback> audio_callback_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SYSTEM_LOOPBACK_LISTENER_H_
