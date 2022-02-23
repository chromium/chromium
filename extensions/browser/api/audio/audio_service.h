// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "extensions/common/api/audio.h"

namespace extensions {

using OutputInfo = std::vector<api::audio::OutputDeviceInfo>;
using InputInfo = std::vector<api::audio::InputDeviceInfo>;
using DeviceIdList = std::vector<std::string>;
using DeviceInfoList = std::vector<api::audio::AudioDeviceInfo>;

class AudioDeviceIdCalculator;

class AudioService {
 public:
  class Observer {
   public:
    // Called when anything changes to the audio device configuration.
    virtual void OnDeviceChanged() = 0;

    // Called when the sound level of an active audio device changes.
    virtual void OnLevelChanged(const std::string& id, int level) = 0;

    // Called when the mute state of audio input/output changes.
    virtual void OnMuteChanged(bool is_input, bool is_muted) = 0;

    // Called when the audio devices change, either new devices being added, or
    // existing devices being removed.
    virtual void OnDevicesChanged(const DeviceInfoList&) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Creates a platform-specific AudioService instance.
  static AudioService* CreateInstance(AudioDeviceIdCalculator* id_calculator);

  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  virtual ~AudioService() {}

  // Called by listeners to this service to add/remove themselves as observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Start to query audio device information. Should be called on UI thread.
  // Populates |output_info_out| and |input_info_out| with the results.
  // Returns true on success.
  // DEPRECATED: Use |GetDevices| instead.
  virtual bool GetInfo(OutputInfo* output_info_out,
                       InputInfo* input_info_out) = 0;

  // Retrieves list of audio devices that satisfy |filter|. Populates
  // |devices_out| with retrieved devices.
  // If |filter->is_active| is set, |devices_out| will contain only devices
  // whose is-active state matches |filter->is_active| value.
  // If |filter->stream_types| is set, |devices_out| will contain only devices
  // whose stream type (INPUT for input devices, OUTPUT for output devices) is
  // contained in |filter->stream_types|.
  // Returns whether the list of devices was successfully retrieved.
  virtual bool GetDevices(const api::audio::DeviceFilter* filter,
                          DeviceInfoList* devices_out) = 0;

  // Sets set of active inputs to devices defined by IDs in |input_devices|,
  // and set of active outputs to devices defined by IDs in |output_devices|.
  // If either of |input_devices| or |output_devices| is not set, associated
  // set of active devices will remain unchanged.
  // If either list is empty, all active devices of associated type will be
  // deactivated.
  // Returns whether the operation succeeded - on failure there will be no
  // changes to active devices.
  // Note that device ID lists should contain only existing device ID of
  // appropriate type in order for the method to succeed.
  virtual bool SetActiveDeviceLists(
      const std::unique_ptr<DeviceIdList>& input_devices,
      const std::unique_ptr<DeviceIdList>& output_devives) = 0;

  // Sets the active devices to the devices specified by |device_list|.
  // It can pass in the "complete" active device list of either input
  // devices, or output devices, or both. If only input devices are passed in,
  // it will only change the input devices' active status, output devices will
  // NOT be changed; similarly for the case if only output devices are passed.
  // If the devices specified in |new_active_ids| are already active, they will
  // remain active. Otherwise, the old active devices will be de-activated
  // before we activate the new devices with the same type(input/output).
  virtual void SetActiveDevices(const DeviceIdList& device_list) = 0;

  // Set the sound level properties (volume or gain) of a device.
  virtual bool SetDeviceSoundLevel(const std::string& device_id,
                                   int volume,
                                   int gain) = 0;

  // Sets the mute property of a device.
  virtual bool SetMuteForDevice(const std::string& device_id, bool value) = 0;

  // Sets mute property for audio input (if |is_input| is true) or output (if
  // |is_input| is false).
  virtual bool SetMute(bool is_input, bool value) = 0;

  // Gets mute property for audio input (if |is_input| is true) or output (if
  // |is_input| is false).
  // The mute value is returned via |mute| argument.
  // The method returns whether the value was successfully fetched.
  virtual bool GetMute(bool is_input, bool* mute) = 0;

 protected:
  AudioService() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_H_
