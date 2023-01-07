// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultOutputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultInputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDevicesPropertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress kPropertyOutputSourceChanged = {
    kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster};

const AudioObjectPropertyAddress kPropertyInputSourceChanged = {
    kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster};

class AudioDeviceListenerMac::PropertyListener {
 public:
  PropertyListener(AudioObjectID monitored_object,
                   const AudioObjectPropertyAddress* property,
                   base::RepeatingClosure callback)
      : monitored_object_(monitored_object),
        address_(property),
        callback_(std::move(callback)) {}

  AudioObjectID monitored_object() const { return monitored_object_; }
  const base::RepeatingClosure& callback() const { return callback_; }
  const AudioObjectPropertyAddress* property() const { return address_; }

 private:
  AudioObjectID monitored_object_;
  raw_ptr<const AudioObjectPropertyAddress> address_;
  base::RepeatingClosure callback_;
};

// Callback from the system when an event occurs; this must be called on the
// MessageLoop that created the AudioManager.
// static
OSStatus AudioDeviceListenerMac::OnEvent(
    AudioObjectID object,
    UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[],
    void* context) {
  PropertyListener* listener = static_cast<PropertyListener*>(context);
  if (object != listener->monitored_object())
    return noErr;

  for (UInt32 i = 0; i < num_addresses; ++i) {
    if (addresses[i].mSelector == listener->property()->mSelector &&
        addresses[i].mScope == listener->property()->mScope &&
        addresses[i].mElement == listener->property()->mElement && context) {
      listener->callback().Run();
      break;
    }
  }

  return noErr;
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::RepeatingClosure listener_cb,
    bool monitor_default_input,
    bool monitor_addition_removal,
    bool monitor_sources)
    : weak_factory_(this) {
  listener_cb_ = std::move(listener_cb);

  // Changes to the default output device are always monitored.
  default_output_listener_ = std::make_unique<PropertyListener>(
      kAudioObjectSystemObject, &kDefaultOutputDeviceChangePropertyAddress,
      listener_cb_);
  if (!AddPropertyListener(default_output_listener_.get()))
    default_output_listener_.reset();

  if (monitor_default_input) {
    default_input_listener_ = std::make_unique<PropertyListener>(
        kAudioObjectSystemObject, &kDefaultInputDeviceChangePropertyAddress,
        listener_cb_);
    if (!AddPropertyListener(default_input_listener_.get()))
      default_input_listener_.reset();
  }
  if (monitor_addition_removal) {
    addition_removal_listener_ = std::make_unique<PropertyListener>(
        kAudioObjectSystemObject, &kDevicesPropertyAddress,
        monitor_sources ? media::BindToCurrentLoop(base::BindRepeating(
                              &AudioDeviceListenerMac::OnDevicesAddedOrRemoved,
                              weak_factory_.GetWeakPtr()))
                        : listener_cb_);
    if (!AddPropertyListener(addition_removal_listener_.get()))
      addition_removal_listener_.reset();

    // Sources can be monitored only if addition/removal is monitored.
    if (monitor_sources)
      UpdateSourceListeners();
  }
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Since we're running on the same CFRunLoop, there can be no outstanding
  // callbacks in flight.
  if (default_output_listener_)
    RemovePropertyListener(default_output_listener_.get());
  if (default_input_listener_)
    RemovePropertyListener(default_input_listener_.get());
  if (addition_removal_listener_)
    RemovePropertyListener(addition_removal_listener_.get());
  for (const auto& entry : source_listeners_)
    RemovePropertyListener(entry.second.get());
}

bool AudioDeviceListenerMac::AddPropertyListener(
    AudioDeviceListenerMac::PropertyListener* property_listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OSStatus result = AudioObjectAddPropertyListener(
      property_listener->monitored_object(), property_listener->property(),
      &AudioDeviceListenerMac::OnEvent, property_listener);
  bool success = result == noErr;
  if (!success)
    OSSTATUS_DLOG(ERROR, result) << "AudioObjectAddPropertyListener() failed!";

  return success;
}

void AudioDeviceListenerMac::RemovePropertyListener(
    AudioDeviceListenerMac::PropertyListener* property_listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OSStatus result = AudioObjectRemovePropertyListener(
      property_listener->monitored_object(), property_listener->property(),
      &AudioDeviceListenerMac::OnEvent, property_listener);
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioObjectRemovePropertyListener() failed!";
}

void AudioDeviceListenerMac::OnDevicesAddedOrRemoved() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateSourceListeners();
  listener_cb_.Run();
}

void AudioDeviceListenerMac::UpdateSourceListeners() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<AudioObjectID> device_ids =
      core_audio_mac::GetAllAudioDeviceIDs();
  for (bool is_input : {true, false}) {
    for (auto device_id : device_ids) {
      const AudioObjectPropertyAddress* property_address =
          is_input ? &kPropertyInputSourceChanged
                   : &kPropertyOutputSourceChanged;
      SourceListenerKey key = {device_id, is_input};
      auto it_key = source_listeners_.find(key);
      bool is_monitored = it_key != source_listeners_.end();
      if (core_audio_mac::GetDeviceSource(device_id, is_input)) {
        if (!is_monitored) {
          // Start monitoring if the device has source and is not currently
          // being monitored.
          std::unique_ptr<PropertyListener> source_listener =
              std::make_unique<PropertyListener>(device_id, property_address,
                                                 listener_cb_);
          if (AddPropertyListener(source_listener.get())) {
            source_listeners_[key] = std::move(source_listener);
          } else {
            source_listener.reset();
          }
        }
      } else if (is_monitored) {
        // Stop monitoring if the device has no source but is currently being
        // monitored.
        RemovePropertyListener(it_key->second.get());
        source_listeners_.erase(it_key);
      }
    }
  }
}

}  // namespace media
