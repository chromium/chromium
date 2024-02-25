// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"

namespace media {

// AudioDeviceListenerMac facilitates execution of device listener callbacks
// issued via CoreAudio.
class MEDIA_EXPORT AudioDeviceListenerMac {
 public:
  // |listener_cb| will be called when a device change occurs; it's a permanent
  // callback and must outlive AudioDeviceListenerMac.  Note that |listener_cb|
  // might not be executed on the same thread as construction.
  static std::unique_ptr<AudioDeviceListenerMac> Create(
      base::RepeatingClosure listener_cb,
      bool monitor_output_sample_rate_changes,
      bool monitor_default_input,
      bool monitor_addition_removal,
      bool monitor_sources);

  AudioDeviceListenerMac(const AudioDeviceListenerMac&) = delete;
  AudioDeviceListenerMac& operator=(const AudioDeviceListenerMac&) = delete;

  // Virtual for overriding in tests.
  virtual ~AudioDeviceListenerMac();

 private:
  friend class AudioDeviceListenerMacTest;
  friend class AudioDeviceListenerMacUnderTest;
  class PropertyListener;

  struct PropertyListenerDeleter {
    void operator()(PropertyListener* listener);
  };

  using PropertyListenerPtr =
      std::unique_ptr<PropertyListener, PropertyListenerDeleter>;

  static const AudioObjectPropertyAddress
      kDefaultOutputDeviceChangePropertyAddress;
  static const AudioObjectPropertyAddress
      kDefaultInputDeviceChangePropertyAddress;
  static const AudioObjectPropertyAddress kDevicesPropertyAddress;
  static const AudioObjectPropertyAddress kPropertyOutputSampleRateChanged;
  static const AudioObjectPropertyAddress kPropertyOutputSourceChanged;
  static const AudioObjectPropertyAddress kPropertyInputSourceChanged;

  AudioDeviceListenerMac(base::RepeatingClosure listener_cb,
                         bool monitor_output_sample_rate_changes,
                         bool monitor_default_input,
                         bool monitor_addition_removal,
                         bool monitor_sources);

  // Must be called only once after constructor.
  void CreatePropertyListeners();

  void RunCallback();
  void UpdateDevicePropertyListeners();
  void OnDevicesAddedOrRemoved();
  void UpdateSourceListeners(const std::vector<AudioObjectID>& device_ids);
  void UpdateOutputSampleRateListeners(
      const std::vector<AudioObjectID>& device_ids);

  PropertyListenerPtr CreatePropertyListener(
      AudioObjectID monitored_object,
      const AudioObjectPropertyAddress* property,
      base::RepeatingClosure listener_cb);

  // Virtual for testing.
  virtual std::vector<AudioObjectID> GetAllAudioDeviceIDs();
  virtual bool IsOutputDevice(AudioObjectID id);
  virtual std::optional<uint32_t> GetDeviceSource(AudioObjectID device_id,
                                                  bool is_input);
  virtual OSStatus AddPropertyListener(
      AudioObjectID inObjectID,
      const AudioObjectPropertyAddress* inAddress,
      AudioObjectPropertyListenerProc inListener,
      void* inClientData);
  virtual OSStatus RemovePropertyListener(
      AudioObjectID inObjectID,
      const AudioObjectPropertyAddress* inAddress,
      AudioObjectPropertyListenerProc inListener,
      void* inClientData);

  std::vector<void*> GetPropertyListenersForTesting() const;

  static OSStatus SimulateEventForTesting(
      AudioObjectID object,
      UInt32 num_addresses,
      const AudioObjectPropertyAddress addresses[],
      void* context);

  const base::RepeatingClosure listener_cb_;
  PropertyListenerPtr default_output_listener_;

  const bool monitor_default_input_;
  PropertyListenerPtr default_input_listener_;

  const bool monitor_addition_removal_;
  PropertyListenerPtr addition_removal_listener_;

  const bool monitor_output_sample_rate_changes_;
  using OutputSampleRateListenerMap =
      base::flat_map<AudioObjectID, PropertyListenerPtr>;
  OutputSampleRateListenerMap output_sample_rate_listeners_;

  const bool monitor_sources_;
  using SourceListenerKey = std::pair<AudioObjectID, bool>;
  using SourceListenerMap =
      base::flat_map<SourceListenerKey, PropertyListenerPtr>;
  SourceListenerMap source_listeners_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
