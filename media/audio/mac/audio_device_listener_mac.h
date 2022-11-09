// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_

#include <CoreAudio/AudioHardware.h>

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
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
  AudioDeviceListenerMac(base::RepeatingClosure listener_cb,
                         bool monitor_default_input = false,
                         bool monitor_addition_removal = false,
                         bool monitor_sources = false);

  AudioDeviceListenerMac(const AudioDeviceListenerMac&) = delete;
  AudioDeviceListenerMac& operator=(const AudioDeviceListenerMac&) = delete;

  ~AudioDeviceListenerMac();

 private:
  friend class AudioDeviceListenerMacTest;
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

  void RunCallback();
  void OnDevicesAddedOrRemoved();
  void UpdateSourceListeners();

  std::vector<void*> GetPropertyListenersForTesting() const;

  static OSStatus SimulateEventForTesting(
      AudioObjectID object,
      UInt32 num_addresses,
      const AudioObjectPropertyAddress addresses[],
      void* context);

  const base::RepeatingClosure listener_cb_;
  PropertyListenerPtr default_output_listener_;
  PropertyListenerPtr default_input_listener_;
  PropertyListenerPtr addition_removal_listener_;

  using SourceListenerKey = std::pair<AudioObjectID, bool>;
  using SourceListenerMap =
      base::flat_map<SourceListenerKey, PropertyListenerPtr>;
  SourceListenerMap source_listeners_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_DEVICE_LISTENER_MAC_H_
