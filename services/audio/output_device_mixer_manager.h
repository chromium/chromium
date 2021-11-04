// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_
#define SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "media/base/audio_parameters.h"
#include "services/audio/device_output_listener.h"
#include "services/audio/output_device_mixer.h"
#include "services/audio/reference_output.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
class AudioManager;
class AudioOutputStream;
}  // namespace media

namespace audio {

// Creates OutputDeviceMixers as needed, when playback is requested through
// MakeOutputStream(). OutputDeviceMixers are destroyed on device change, or
// when the Audio service shuts down, but are not cleaned up otherwise.
// Listening to a device has no effect, until that device's OutputDeviceMixer is
// created and playback has started.
class OutputDeviceMixerManager : public DeviceOutputListener {
 public:
  OutputDeviceMixerManager(
      media::AudioManager* audio_manager,
      OutputDeviceMixer::CreateCallback create_mixer_callback);
  OutputDeviceMixerManager(const OutputDeviceMixerManager&) = delete;
  OutputDeviceMixerManager& operator=(const OutputDeviceMixerManager&) = delete;
  ~OutputDeviceMixerManager() override;

  // Makes an output stream for the given output device.
  // The returned stream must be closed synchronously from
  // |close_stream_on_device_change|.
  media::AudioOutputStream* MakeOutputStream(
      const std::string& device_id,
      const media::AudioParameters& params,
      base::OnceClosure close_stream_on_device_change);

  // DeviceOutputListener implementation
  void StartListening(ReferenceOutput::Listener* listener,
                      const std::string& device_id) final;
  void StopListening(ReferenceOutput::Listener* listener,
                     const std::string& device_id) final;

 private:
  friend class OutputDeviceMixerManagerTest;

  using ListenerSet = std::set<ReferenceOutput::Listener*>;
  using OutputDeviceMixers = std::vector<std::unique_ptr<OutputDeviceMixer>>;
  using DeviceToListenersMap = base::flat_map<std::string, ListenerSet>;

  // Forwards device change notifications to OutputDeviceMixers.
  void OnDeviceChange();

  // Returns the current physical default device ID, which can change after
  // OnDeviceChange().
  const std::string& GetCurrentDefaultDeviceId();

  // Returns |device_id|, or the current default device ID if |device_id| is
  // the default ID.
  const std::string& ConvertToPhysicalDeviceId(const std::string& device_id);

  media::AudioOutputStream* CreateDeviceListenerStream(
      base::OnceClosure on_device_change_callback,
      const std::string& device_id,
      const media::AudioParameters& params);

  void AttachListenersById(const std::string& device_id,
                           OutputDeviceMixer* mixer);

  // Returns a mixer if it exists, or nullptr otherwise.
  OutputDeviceMixer* FindMixer(const std::string& physical_device_id);

  // Creates and returns a new mixer, or nullptr if the creation failed.
  OutputDeviceMixer* AddMixer(const std::string& physical_device_id);

  SEQUENCE_CHECKER(owning_sequence_);
  absl::optional<std::string> current_default_device_id_ = absl::nullopt;
  media::AudioManager* const audio_manager_;
  OutputDeviceMixer::CreateCallback create_mixer_callback_;
  OutputDeviceMixers output_device_mixers_;
  DeviceToListenersMap device_id_to_listeners_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_
