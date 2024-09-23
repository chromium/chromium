// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/mac/audio_device_listener_mac.h"

#include <optional>
#include <vector>

#include "base/apple/osstatus_logging.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mac/core_audio_util_mac.h"

namespace media {

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultOutputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDefaultInputDeviceChangePropertyAddress = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kDevicesPropertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kPropertyOutputSampleRateChanged = {
        kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kPropertyOutputSourceChanged = {
        kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress
    AudioDeviceListenerMac::kPropertyInputSourceChanged = {
        kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain};

class AudioDeviceListenerMac::PropertyListener {
 public:
  using SystemCallback =
      base::OnceCallback<OSStatus(AudioObjectID inObjectID,
                                  const AudioObjectPropertyAddress* inAddress,
                                  AudioObjectPropertyListenerProc inListener,
                                  void* inClientData)>;

  // |remove_listener_callback| is guaranteed to be synchronously called in the
  // deleter.
  static std::unique_ptr<PropertyListener, PropertyListenerDeleter> Create(
      AudioObjectID monitored_object,
      const AudioObjectPropertyAddress* property,
      base::RepeatingClosure on_change_callback,
      SystemCallback add_listener_callback,
      SystemCallback remove_listener_callback) {
    std::unique_ptr<PropertyListener, PropertyListenerDeleter> listener(
        new PropertyListener(monitored_object, property));

    if (!listener->StartListening(std::move(on_change_callback),
                                  std::move(add_listener_callback),
                                  std::move(remove_listener_callback)))
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
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    weak_this_for_events_ = weak_factory_.GetWeakPtr();
  }

  ~PropertyListener() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!callback_);
    DCHECK(remove_listener_callback_.is_null());
  }

  bool StartListening(base::RepeatingClosure callback,
                      SystemCallback add_listener_callback,
                      SystemCallback remove_listener_callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback_.is_null());

    OSStatus result =
        std::move(add_listener_callback)
            .Run(monitored_object_, property_,
                 &AudioDeviceListenerMac::PropertyListener::OnEvent, this);
    if (noErr == result) {
      callback_ = std::move(callback);
      remove_listener_callback_ = std::move(remove_listener_callback);
      return true;
    }

    OSSTATUS_DLOG(ERROR, result) << "AddPropertyListener() failed!";
    return false;
  }

  // Returns true if it stopped listening and false if it was not listening and
  // so did not have to stop doing so.
  bool MaybeStopListening() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (callback_.is_null())
      return false;
    DCHECK(!remove_listener_callback_.is_null());
    OSStatus result =
        std::move(remove_listener_callback_)
            .Run(monitored_object_, property_,
                 &AudioDeviceListenerMac::PropertyListener::OnEvent, this);
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "RemovePropertyListener() failed!";
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
  SystemCallback remove_listener_callback_ GUARDED_BY_CONTEXT(thread_checker_);

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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](PropertyListener* listener) {
            CHECK(listener);
            delete listener;
          },
          listener),
      base::Seconds(1));
}
// static
std::unique_ptr<AudioDeviceListenerMac> AudioDeviceListenerMac::Create(
    base::RepeatingClosure listener_cb,
    bool monitor_output_sample_rate_changes,
    bool monitor_default_input,
    bool monitor_addition_removal,
    bool monitor_sources) {
  // No make_unique<> since the constructor is private.
  std::unique_ptr<AudioDeviceListenerMac> device_listener(
      new AudioDeviceListenerMac(
          std::move(listener_cb), monitor_output_sample_rate_changes,
          monitor_default_input, monitor_addition_removal, monitor_sources));
  device_listener->CreatePropertyListeners();
  return device_listener;
}

AudioDeviceListenerMac::~AudioDeviceListenerMac() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

AudioDeviceListenerMac::AudioDeviceListenerMac(
    const base::RepeatingClosure listener_cb,
    bool monitor_output_sample_rate_changes,
    bool monitor_default_input,
    bool monitor_addition_removal,
    bool monitor_sources)
    : listener_cb_(std::move(listener_cb)),
      monitor_default_input_(monitor_default_input),
      monitor_addition_removal_(monitor_addition_removal),
      monitor_output_sample_rate_changes_(monitor_output_sample_rate_changes),
      monitor_sources_(monitor_sources) {
  DVLOG(1) << __func__ << " this=" << this
           << " monitor_output_sample_rate_changes "
           << monitor_output_sample_rate_changes << " monitor_default_input "
           << monitor_default_input << " monitor_addition_removal "
           << " monitor_sources " << monitor_sources;
}

void AudioDeviceListenerMac::CreatePropertyListeners() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!default_output_listener_);

  // Changes to the default output device are always monitored.
  default_output_listener_ = CreatePropertyListener(
      kAudioObjectSystemObject, &kDefaultOutputDeviceChangePropertyAddress,
      listener_cb_);

  if (monitor_default_input_) {
    default_input_listener_ = CreatePropertyListener(
        kAudioObjectSystemObject, &kDefaultInputDeviceChangePropertyAddress,
        listener_cb_);
  }

  if (monitor_addition_removal_ || monitor_output_sample_rate_changes_ ||
      monitor_sources_) {
    addition_removal_listener_ = CreatePropertyListener(
        kAudioObjectSystemObject, &kDevicesPropertyAddress,
        base::BindRepeating(&AudioDeviceListenerMac::OnDevicesAddedOrRemoved,
                            base::Unretained(this)));
    // Even if |addition_removal_listener_| creation failed we still want to
    // monitor at least the devices we see at the moment.
    UpdateDevicePropertyListeners();
  }
}

void AudioDeviceListenerMac::OnDevicesAddedOrRemoved() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateDevicePropertyListeners();
  if (monitor_addition_removal_)
    listener_cb_.Run();
}

void AudioDeviceListenerMac::UpdateDevicePropertyListeners() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<AudioObjectID> device_ids = GetAllAudioDeviceIDs();
  if (monitor_sources_)
    UpdateSourceListeners(device_ids);
  if (monitor_output_sample_rate_changes_)
    UpdateOutputSampleRateListeners(device_ids);
}

void AudioDeviceListenerMac::UpdateSourceListeners(
    const std::vector<AudioObjectID>& device_ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(monitor_sources_);
  DVLOG(1) << __func__ << " this=" << this;

  SourceListenerMap new_listeners;
  for (bool is_input : {true, false}) {
    for (auto device_id : device_ids) {
      // Do not monitor devices which do not have sources.
      if (!GetDeviceSource(device_id, is_input))
        continue;

      SourceListenerKey key = {device_id, is_input};
      auto listener_iter = source_listeners_.find(key);
      if (listener_iter != source_listeners_.end()) {
        // Continue monitoring.
        new_listeners[key] = std::move(listener_iter->second);
        continue;
      }
      // Start monitoring
      const AudioObjectPropertyAddress* property_address =
          is_input ? &kPropertyInputSourceChanged
                   : &kPropertyOutputSourceChanged;
      auto new_listener =
          CreatePropertyListener(device_id, property_address, listener_cb_);
      if (new_listener)
        new_listeners[key] = std::move(new_listener);
    }
  }

  // Drop all the listeners not in |device_ids|.
  source_listeners_.swap(new_listeners);
}

void AudioDeviceListenerMac::UpdateOutputSampleRateListeners(
    const std::vector<AudioObjectID>& device_ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(monitor_output_sample_rate_changes_);

  OutputSampleRateListenerMap new_listeners;
  for (auto device_id : device_ids) {
    if (!IsOutputDevice(device_id))
      continue;

    auto listener_iter = output_sample_rate_listeners_.find(device_id);
    if (listener_iter != output_sample_rate_listeners_.end()) {
      // Continue monitoring.
      new_listeners[device_id] = std::move(listener_iter->second);
      continue;
    }
    // Start monitoring
    auto new_listener = CreatePropertyListener(
        device_id, &kPropertyOutputSampleRateChanged, listener_cb_);
    if (new_listener)
      new_listeners[device_id] = std::move(new_listener);
  }

  // Drop all the listeners not in |device_ids|.
  output_sample_rate_listeners_.swap(new_listeners);

  DVLOG(1) << __func__ << " this=" << this
           << " listener count: " << output_sample_rate_listeners_.size();
}

AudioDeviceListenerMac::PropertyListenerPtr
AudioDeviceListenerMac::CreatePropertyListener(
    AudioObjectID monitored_object,
    const AudioObjectPropertyAddress* property,
    base::RepeatingClosure listener_cb) {
  // Unretained is safe because the callbacks are guaranteed to be called only
  // while |this| holds the listener.
  return PropertyListener::Create(
      monitored_object, property, std::move(listener_cb),
      base::BindOnce(&AudioDeviceListenerMac::AddPropertyListener,
                     base::Unretained(this)),
      base::BindOnce(&AudioDeviceListenerMac::RemovePropertyListener,
                     base::Unretained(this)));
}

std::vector<AudioObjectID> AudioDeviceListenerMac::GetAllAudioDeviceIDs() {
  return core_audio_mac::GetAllAudioDeviceIDs();
}

bool AudioDeviceListenerMac::IsOutputDevice(AudioObjectID id) {
  return core_audio_mac::IsOutputDevice(id);
}

std::optional<uint32_t> AudioDeviceListenerMac::GetDeviceSource(
    AudioObjectID device_id,
    bool is_input) {
  return core_audio_mac::GetDeviceSource(device_id, is_input);
}

OSStatus AudioDeviceListenerMac::AddPropertyListener(
    AudioObjectID inObjectID,
    const AudioObjectPropertyAddress* inAddress,
    AudioObjectPropertyListenerProc inListener,
    void* inClientData) {
  return AudioObjectAddPropertyListener(inObjectID, inAddress, inListener,
                                        inClientData);
}

OSStatus AudioDeviceListenerMac::RemovePropertyListener(
    AudioObjectID inObjectID,
    const AudioObjectPropertyAddress* inAddress,
    AudioObjectPropertyListenerProc inListener,
    void* inClientData) {
  return AudioObjectRemovePropertyListener(inObjectID, inAddress, inListener,
                                           inClientData);
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
  for (const auto& listener_pair : output_sample_rate_listeners_)
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
