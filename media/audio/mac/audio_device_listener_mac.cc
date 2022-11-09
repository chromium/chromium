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
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mac/core_audio_util_mac.h"
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
  static std::unique_ptr<PropertyListener, PropertyListenerDeleter> Create(
      AudioObjectID monitored_object,
      const AudioObjectPropertyAddress* property,
      base::RepeatingClosure callback) {
    std::unique_ptr<PropertyListener, PropertyListenerDeleter> listener{
        new PropertyListener(monitored_object, property)};

    if (!listener->StartListening(std::move(callback)))
      listener.reset();

    return listener;
  }

  // Public for testing.
  static OSStatus OnEvent(AudioObjectID object,
                          UInt32 num_addresses,
                          const AudioObjectPropertyAddress addresses[],
                          void* context) {
    if (context) {
      static_cast<PropertyListener*>(context)->ProcessEvent(
          object, std::vector<AudioObjectPropertyAddress>(
                      addresses, addresses + num_addresses));
    }
    return noErr;
  }

 private:
  friend struct PropertyListenerDeleter;

  PropertyListener(AudioObjectID monitored_object,
                   const AudioObjectPropertyAddress* property)
      : monitored_object_(monitored_object),
        property_(property),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    weak_this_for_events_ = weak_factory_.GetWeakPtr();
  }

  ~PropertyListener() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!callback_);
  }

  bool StartListening(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback_.is_null());

    OSStatus result = AudioObjectAddPropertyListener(
        monitored_object_, property_,
        &AudioDeviceListenerMac::PropertyListener::OnEvent, this);
    if (noErr == result) {
      callback_ = std::move(callback);
      return true;
    }

    OSSTATUS_DLOG(ERROR, result) << "AudioObjectAddPropertyListener() failed!";
    return false;
  }

  // Returns true if it stopped listening and false if it was not listening and
  // so did not have to stop doing so.
  bool MaybeStopListening() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (callback_.is_null())
      return false;
    OSStatus result = AudioObjectRemovePropertyListener(
        monitored_object_, property_,
        &AudioDeviceListenerMac::PropertyListener::OnEvent, this);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "AudioObjectRemovePropertyListener() failed!";
    auto callback(std::move(callback_));
    DCHECK(callback_.is_null());
    return true;
  }

  void ProcessEvent(AudioObjectID object,
                    std::vector<AudioObjectPropertyAddress> properties) {
    // This call can come from a different thread (for example it may happen for
    // a device sample rate change notification). We only hope it does not race
    // with the destructor (which we delay by means of a custom deleter), which
    // may happen if a device parameter change notification races with device
    // removal notification for the same device. Protecting |this| with a lock
    // here and in the destructor will likely lead to a deadlock around
    // AudioObjectRemovePropertyListener. So we just attempt to minimize a
    // probablylity of a race by delaying the destructor and doing all the
    // checks on the main thread.
    if (!task_runner_->BelongsToCurrentThread()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&PropertyListener::ProcessEvent, weak_this_for_events_,
                         object, std::move(properties)));
      return;
    }

    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (object != monitored_object_)
      return;

    if (callback_.is_null())
      return;

    for (const auto& property : properties) {
      if (property.mSelector == property_->mSelector &&
          property.mScope == property_->mScope &&
          property.mElement == property_->mElement) {
        callback_.Run();
        break;
      }
    }
  }

  const AudioObjectID monitored_object_;
  const raw_ptr<const AudioObjectPropertyAddress> property_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Ensures the methods run on |task_runner_|.
  THREAD_CHECKER(thread_checker_);

  base::RepeatingClosure callback_ GUARDED_BY_CONTEXT(thread_checker_);

  base::WeakPtr<PropertyListener> weak_this_for_events_;
  base::WeakPtrFactory<PropertyListener> weak_factory_{this};
};

void AudioDeviceListenerMac::PropertyListenerDeleter::operator()(
    PropertyListener* listener) {
  if (!listener)
    return;

  if (!listener->MaybeStopListening()) {
    // The listener was not subscribed for notifications, so there are no
    // notifications in progress. We can delete it immediately.
    delete listener;
    return;
  }

  // The listener has been listening to changes; defer its deletion in case
  // there is a notification in progress on another thread - to avoid a race.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](PropertyListener* listener) {
            CHECK(listener);
            delete listener;
          },
          listener),
      base::Seconds(1));
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::RepeatingClosure listener_cb,
    bool monitor_default_input,
    bool monitor_addition_removal,
    bool monitor_sources)
    : listener_cb_(std::move(listener_cb)) {
  // Changes to the default output device are always monitored.
  default_output_listener_ = PropertyListener::Create(
      kAudioObjectSystemObject, &kDefaultOutputDeviceChangePropertyAddress,
      listener_cb_);

  if (monitor_default_input) {
    default_input_listener_ = PropertyListener::Create(
        kAudioObjectSystemObject, &kDefaultInputDeviceChangePropertyAddress,
        listener_cb_);
  }
  if (monitor_addition_removal) {
    addition_removal_listener_ = PropertyListener::Create(
        kAudioObjectSystemObject, &kDevicesPropertyAddress,
        monitor_sources ? base::BindRepeating(
                              &AudioDeviceListenerMac::OnDevicesAddedOrRemoved,
                              base::Unretained(this))
                        : listener_cb_);

    // Sources can be monitored only if addition/removal is monitored.
    if (monitor_sources)
      UpdateSourceListeners();
  }
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
          auto source_listener = PropertyListener::Create(
              device_id, property_address, listener_cb_);
          if (source_listener) {
            source_listeners_[key] = std::move(source_listener);
          }
        }
      } else if (is_monitored) {
        // Stop monitoring if the device has no source but is currently being
        // monitored.
        source_listeners_.erase(it_key);
      }
    }
  }
}

std::vector<void*> AudioDeviceListenerMac::GetPropertyListenersForTesting()
    const {
  std::vector<void*> listeners;
  if (default_output_listener_)
    listeners.push_back(default_output_listener_.get());
  if (default_input_listener_)
    listeners.push_back(default_input_listener_.get());
  if (addition_removal_listener_)
    listeners.push_back(addition_removal_listener_.get());
  for (const auto& listener_pair : source_listeners_)
    listeners.push_back(listener_pair.second.get());
  return listeners;
}

// static
OSStatus AudioDeviceListenerMac::SimulateEventForTesting(
    AudioObjectID object,
    UInt32 num_addresses,
    const AudioObjectPropertyAddress addresses[],
    void* context) {
  return PropertyListener::OnEvent(object, num_addresses, addresses, context);
}

}  // namespace media
