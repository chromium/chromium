// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_
#define SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/audio_parameters.h"
#include "services/audio/output_device_mixer.h"
#include "services/audio/reference_output.h"
#include "services/audio/reference_signal_provider.h"

namespace media {
class AudioManager;
class AudioOutputStream;
}  // namespace media

namespace audio {

class OutputDeviceMixerReferenceProvider;

// Creates OutputDeviceMixers as needed, when playback is requested through
// MakeOutputStream(). OutputDeviceMixers are destroyed on device change, or
// when the Audio service shuts down, but are not cleaned up otherwise.
// Listening to a device has no effect, until that device's OutputDeviceMixer is
// created and playback has started.
class OutputDeviceMixerManager : public ReferenceSignalProviderFactory {
 public:
  // Outcome of detecting that the default or communications device changed
  // without a notification. Recorded to
  // Media.Audio.OutputDeviceMixerManager.MissedDeviceChange.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(AudioOutputDeviceMixerManagerMissedDeviceChange)
  enum class MissedDeviceChange {
    kDefaultDevice = 0,
    kCommunicationsDevice = 1,
    kDefaultAndCommunicationsDevices = 2,
    // AudioManager reported an empty ID while a physical ID was cached. This
    // is treated as a failed query rather than as a device change.
    kEmptyDeviceIdIgnored = 3,
    kMaxValue = kEmptyDeviceIdIgnored,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:AudioOutputDeviceMixerManagerMissedDeviceChange)

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

  // ReferenceSignalProviderFactory implementation. Needs to be called on the
  // same sequence that OutputDeviceMixerManager was created on.
  std::unique_ptr<ReferenceSignalProvider> GetReferenceSignalProvider() final;

 private:
  friend class OutputDeviceMixerManagerTest;
  friend class OutputDeviceMixerReferenceProvider;

  // Corresponds to the ReferenceSignalProvider interface
  void StartListening(ReferenceOutput::Listener* listener,
                      const std::string& device_id);
  void StopListening(ReferenceOutput::Listener* listener);

  using OutputDeviceMixers = std::vector<std::unique_ptr<OutputDeviceMixer>>;
  using ListenerToDeviceMap =
      base::flat_map<raw_ptr<ReferenceOutput::Listener, DanglingUntriaged>,
                     std::string>;

  // Refreshes the cached default and communications device IDs and calls
  // ProcessDeviceChange().
  void OnDeviceChange();

  // Tears down existing OutputDeviceMixers and notifies them of a device
  // change so their clients can recreate their streams.
  void ProcessDeviceChange();

  // Device change notifications reach this class only through mixer-owned
  // output streams, so a change of the default or communications device while
  // none of them is open goes unnoticed, and ToMixerDeviceId() would route
  // physical IDs to the wrong mixer. Detects such a missed change by comparing
  // the cached IDs with the ones reported by AudioManager, and processes it via
  // ProcessDeviceChange(). An empty reported ID is not a change: it indicates a
  // failed query. Must be called before routing decisions.
  void MaybeProcessMissedDeviceChange();

  // Helper function which maps physical and reserved IDs to normalized mixer
  // device IDs. Physical IDs matching the current "default" or "communications"
  // physical devices will be converted to reserved IDs
  // (kNormalizedDefaultDeviceId or kCommunicationsDeviceId), ensuring we only
  // create one mixer per device.
  std::string ToMixerDeviceId(const std::string& device_id);

  // Returns a callback that call OnDeviceChange(), and that can be cancelled
  // through invalidating |device_change_weak_ptr_factory_|.
  // This is split out into its own method to simplify UTs.
  base::OnceClosure GetOnDeviceChangeCallback();

  media::AudioOutputStream* CreateMixerOwnedStream(
      const std::string& device_id,
      const media::AudioParameters& params);

  media::AudioOutputStream* CreateDeviceListenerStream(
      base::OnceClosure on_device_change_callback,
      const std::string& device_id,
      const media::AudioParameters& params);

  // Returns a mixer if it exists, or nullptr otherwise.
  OutputDeviceMixer* FindMixer(const std::string& physical_device_id);

  // Creates and returns a new mixer, or nullptr if the creation failed.
  OutputDeviceMixer* AddMixer(const std::string& physical_device_id);

  void StartNewListener(ReferenceOutput::Listener* listener,
                        const std::string& device_id);

  bool IsNormalizedIfDefault(const std::string& device_id);

  SEQUENCE_CHECKER(owning_sequence_);
  const raw_ptr<media::AudioManager> audio_manager_;

  // Physical device ID of the current default device, or kNormalizedDefaultId
  // if not supported by the platform.
  std::string current_default_device_id_;

  // Physical device ID of the current communication device, or an empty string
  // if not supported by the platform or not configured.
  std::string current_communication_device_id_;

  OutputDeviceMixer::CreateCallback create_mixer_callback_;
  OutputDeviceMixers output_device_mixers_;
  ListenerToDeviceMap listener_registration_;

  // True while ProcessDeviceChange() runs. Streams recreated synchronously
  // during it skip MaybeProcessMissedDeviceChange(), so a newer device change
  // can't start a nested rebuild that tears down the mixers being created.
  bool processing_device_change_ = false;

  base::WeakPtrFactory<OutputDeviceMixerManager> device_change_weak_ptr_factory_
      GUARDED_BY_CONTEXT(owning_sequence_);
};
}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_DEVICE_MIXER_MANAGER_H_
