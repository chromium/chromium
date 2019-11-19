// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service.h"

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"

using content::BrowserThread;

namespace extensions {

using api::audio::OutputDeviceInfo;
using api::audio::InputDeviceInfo;
using api::audio::AudioDeviceInfo;

namespace {

api::audio::DeviceType GetAsAudioApiDeviceType(chromeos::AudioDeviceType type) {
  switch (type) {
    case chromeos::AUDIO_TYPE_HEADPHONE:
      return api::audio::DEVICE_TYPE_HEADPHONE;
    case chromeos::AUDIO_TYPE_MIC:
      return api::audio::DEVICE_TYPE_MIC;
    case chromeos::AUDIO_TYPE_USB:
      return api::audio::DEVICE_TYPE_USB;
    case chromeos::AUDIO_TYPE_BLUETOOTH:
    case chromeos::AUDIO_TYPE_BLUETOOTH_NB_MIC:
      return api::audio::DEVICE_TYPE_BLUETOOTH;
    case chromeos::AUDIO_TYPE_HDMI:
      return api::audio::DEVICE_TYPE_HDMI;
    case chromeos::AUDIO_TYPE_INTERNAL_SPEAKER:
      return api::audio::DEVICE_TYPE_INTERNAL_SPEAKER;
    case chromeos::AUDIO_TYPE_INTERNAL_MIC:
      return api::audio::DEVICE_TYPE_INTERNAL_MIC;
    case chromeos::AUDIO_TYPE_FRONT_MIC:
      return api::audio::DEVICE_TYPE_FRONT_MIC;
    case chromeos::AUDIO_TYPE_REAR_MIC:
      return api::audio::DEVICE_TYPE_REAR_MIC;
    case chromeos::AUDIO_TYPE_KEYBOARD_MIC:
      return api::audio::DEVICE_TYPE_KEYBOARD_MIC;
    case chromeos::AUDIO_TYPE_HOTWORD:
      return api::audio::DEVICE_TYPE_HOTWORD;
    case chromeos::AUDIO_TYPE_LINEOUT:
      return api::audio::DEVICE_TYPE_LINEOUT;
    case chromeos::AUDIO_TYPE_POST_MIX_LOOPBACK:
      return api::audio::DEVICE_TYPE_POST_MIX_LOOPBACK;
    case chromeos::AUDIO_TYPE_POST_DSP_LOOPBACK:
      return api::audio::DEVICE_TYPE_POST_DSP_LOOPBACK;
    case chromeos::AUDIO_TYPE_OTHER:
      return api::audio::DEVICE_TYPE_OTHER;
  }

  NOTREACHED();
  return api::audio::DEVICE_TYPE_OTHER;
}

}  // namespace

class AudioServiceImpl : public AudioService,
                         public chromeos::CrasAudioHandler::AudioObserver {
 public:
  explicit AudioServiceImpl(AudioDeviceIdCalculator* id_calculator);
  ~AudioServiceImpl() override;

  // Called by listeners to this service to add/remove themselves as observers.
  void AddObserver(AudioService::Observer* observer) override;
  void RemoveObserver(AudioService::Observer* observer) override;

  // Start to query audio device information.
  bool GetInfo(OutputInfo* output_info_out, InputInfo* input_info_out) override;
  bool GetDevices(const api::audio::DeviceFilter* filter,
                  DeviceInfoList* devices_out) override;
  void SetActiveDevices(const DeviceIdList& device_list) override;
  bool SetActiveDeviceLists(
      const std::unique_ptr<DeviceIdList>& input_devices,
      const std::unique_ptr<DeviceIdList>& output_devives) override;
  bool SetDeviceSoundLevel(const std::string& device_id,
                           int volume,
                           int gain) override;
  bool SetMuteForDevice(const std::string& device_id, bool value) override;
  bool SetMute(bool is_input, bool value) override;
  bool GetMute(bool is_input, bool* value) override;

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnOutputNodeVolumeChanged(uint64_t id, int volume) override;
  void OnInputNodeGainChanged(uint64_t id, int gain) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnInputMuteChanged(bool mute_on) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

 private:
  void NotifyDeviceChanged();
  void NotifyLevelChanged(uint64_t id, int level);
  void NotifyMuteChanged(bool is_input, bool is_muted);
  void NotifyDevicesChanged();

  uint64_t GetIdFromStr(const std::string& id_str);
  bool GetAudioNodeIdList(const DeviceIdList& ids,
                          bool is_input,
                          chromeos::CrasAudioHandler::NodeIdList* node_ids);
  AudioDeviceInfo ToAudioDeviceInfo(const chromeos::AudioDevice& device);

  // List of observers.
  base::ObserverList<AudioService::Observer>::Unchecked observer_list_;

  chromeos::CrasAudioHandler* cras_audio_handler_;

  AudioDeviceIdCalculator* id_calculator_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioServiceImpl);
};

AudioServiceImpl::AudioServiceImpl(AudioDeviceIdCalculator* id_calculator)
    : cras_audio_handler_(chromeos::CrasAudioHandler::Get()),
      id_calculator_(id_calculator) {
  CHECK(id_calculator_);

  if (cras_audio_handler_)
    cras_audio_handler_->AddAudioObserver(this);
}

AudioServiceImpl::~AudioServiceImpl() {
  // The CrasAudioHandler global instance may have already been destroyed, so
  // do not used the cached pointer here.
  if (chromeos::CrasAudioHandler::Get())
    chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void AudioServiceImpl::AddObserver(AudioService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AudioServiceImpl::RemoveObserver(AudioService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool AudioServiceImpl::GetInfo(OutputInfo* output_info_out,
                               InputInfo* input_info_out) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(cras_audio_handler_);
  DCHECK(output_info_out);
  DCHECK(input_info_out);

  if (!cras_audio_handler_)
    return false;

  chromeos::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (!devices[i].is_input) {
      OutputDeviceInfo info;
      info.id = base::NumberToString(devices[i].id);
      info.name = devices[i].device_name + ": " + devices[i].display_name;
      info.is_active = devices[i].active;
      info.volume =
          cras_audio_handler_->GetOutputVolumePercentForDevice(devices[i].id);
      info.is_muted =
          cras_audio_handler_->IsOutputMutedForDevice(devices[i].id);
      output_info_out->push_back(std::move(info));
    } else {
      InputDeviceInfo info;
      info.id = base::NumberToString(devices[i].id);
      info.name = devices[i].device_name + ": " + devices[i].display_name;
      info.is_active = devices[i].active;
      info.gain =
          cras_audio_handler_->GetInputGainPercentForDevice(devices[i].id);
      info.is_muted = cras_audio_handler_->IsInputMutedForDevice(devices[i].id);
      input_info_out->push_back(std::move(info));
    }
  }
  return true;
}

bool AudioServiceImpl::GetDevices(const api::audio::DeviceFilter* filter,
                                  DeviceInfoList* devices_out) {
  if (!cras_audio_handler_)
    return false;

  chromeos::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);

  bool accept_input =
      !(filter && filter->stream_types) ||
      base::Contains(*filter->stream_types, api::audio::STREAM_TYPE_INPUT);
  bool accept_output =
      !(filter && filter->stream_types) ||
      base::Contains(*filter->stream_types, api::audio::STREAM_TYPE_OUTPUT);

  for (const auto& device : devices) {
    if (filter && filter->is_active && *filter->is_active != device.active)
      continue;
    if (device.is_input && !accept_input)
      continue;
    if (!device.is_input && !accept_output)
      continue;
    devices_out->push_back(ToAudioDeviceInfo(device));
  }

  return true;
}

void AudioServiceImpl::SetActiveDevices(const DeviceIdList& device_list) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return;

  chromeos::CrasAudioHandler::NodeIdList id_list;
  for (const auto& id : device_list) {
    const chromeos::AudioDevice* device =
        cras_audio_handler_->GetDeviceFromId(GetIdFromStr(id));
    if (device)
      id_list.push_back(device->id);
  }
  cras_audio_handler_->ChangeActiveNodes(id_list);
}

bool AudioServiceImpl::SetActiveDeviceLists(
    const std::unique_ptr<DeviceIdList>& input_ids,
    const std::unique_ptr<DeviceIdList>& output_ids) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  chromeos::CrasAudioHandler::NodeIdList input_nodes;
  if (input_ids.get() && !GetAudioNodeIdList(*input_ids, true, &input_nodes))
    return false;

  chromeos::CrasAudioHandler::NodeIdList output_nodes;
  if (output_ids.get() &&
      !GetAudioNodeIdList(*output_ids, false, &output_nodes)) {
    return false;
  }

  bool success = true;
  if (output_ids.get()) {
    success = cras_audio_handler_->SetActiveOutputNodes(output_nodes);
    DCHECK(success);
  }

  if (input_ids.get()) {
    success = success && cras_audio_handler_->SetActiveInputNodes(input_nodes);
    DCHECK(success);
  }
  return success;
}

bool AudioServiceImpl::SetDeviceSoundLevel(const std::string& device_id,
                                           int volume,
                                           int gain) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  const chromeos::AudioDevice* device =
      cras_audio_handler_->GetDeviceFromId(GetIdFromStr(device_id));
  if (!device)
    return false;

  if (!device->is_input && volume != -1) {
    cras_audio_handler_->SetVolumeGainPercentForDevice(device->id, volume);
    return true;
  } else if (device->is_input && gain != -1) {
    cras_audio_handler_->SetVolumeGainPercentForDevice(device->id, gain);
    return true;
  }

  return false;
}

bool AudioServiceImpl::SetMuteForDevice(const std::string& device_id,
                                        bool value) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  const chromeos::AudioDevice* device =
      cras_audio_handler_->GetDeviceFromId(GetIdFromStr(device_id));
  if (!device)
    return false;

  cras_audio_handler_->SetMuteForDevice(device->id, value);
  return true;
}

bool AudioServiceImpl::SetMute(bool is_input, bool value) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  if (is_input)
    cras_audio_handler_->SetInputMute(value);
  else
    cras_audio_handler_->SetOutputMute(value);
  return true;
}

bool AudioServiceImpl::GetMute(bool is_input, bool* value) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_)
    return false;

  if (is_input)
    *value = cras_audio_handler_->IsInputMuted();
  else
    *value = cras_audio_handler_->IsOutputMuted();
  return true;
}

uint64_t AudioServiceImpl::GetIdFromStr(const std::string& id_str) {
  uint64_t device_id;
  if (!base::StringToUint64(id_str, &device_id))
    return 0;
  else
    return device_id;
}

bool AudioServiceImpl::GetAudioNodeIdList(
    const DeviceIdList& ids,
    bool is_input,
    chromeos::CrasAudioHandler::NodeIdList* node_ids) {
  for (const auto& device_id : ids) {
    const chromeos::AudioDevice* device =
        cras_audio_handler_->GetDeviceFromId(GetIdFromStr(device_id));
    if (!device)
      return false;
    if (device->is_input != is_input)
      return false;
    node_ids->push_back(device->id);
  }
  return true;
}

AudioDeviceInfo AudioServiceImpl::ToAudioDeviceInfo(
    const chromeos::AudioDevice& device) {
  AudioDeviceInfo info;
  info.id = base::NumberToString(device.id);
  info.stream_type = device.is_input
                         ? extensions::api::audio::STREAM_TYPE_INPUT
                         : extensions::api::audio::STREAM_TYPE_OUTPUT;
  info.device_type = GetAsAudioApiDeviceType(device.type);
  info.display_name = device.display_name;
  info.device_name = device.device_name;
  info.is_active = device.active;
  info.level =
      device.is_input
          ? cras_audio_handler_->GetOutputVolumePercentForDevice(device.id)
          : cras_audio_handler_->GetInputGainPercentForDevice(device.id);
  info.stable_device_id = std::make_unique<std::string>(
      id_calculator_->GetStableDeviceId(device.stable_device_id));

  return info;
}

void AudioServiceImpl::OnOutputNodeVolumeChanged(uint64_t id, int volume) {
  NotifyLevelChanged(id, volume);
}

void AudioServiceImpl::OnOutputMuteChanged(bool mute_on) {
  NotifyMuteChanged(false, mute_on);
}

void AudioServiceImpl::OnInputNodeGainChanged(uint64_t id, int gain) {
  NotifyLevelChanged(id, gain);
}

void AudioServiceImpl::OnInputMuteChanged(bool mute_on) {
  NotifyMuteChanged(true, mute_on);
}

void AudioServiceImpl::OnAudioNodesChanged() {
  NotifyDevicesChanged();
}

void AudioServiceImpl::OnActiveOutputNodeChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::OnActiveInputNodeChanged() {
  NotifyDeviceChanged();
}

void AudioServiceImpl::NotifyDeviceChanged() {
  for (auto& observer : observer_list_)
    observer.OnDeviceChanged();
}

void AudioServiceImpl::NotifyLevelChanged(uint64_t id, int level) {
  for (auto& observer : observer_list_)
    observer.OnLevelChanged(base::NumberToString(id), level);

  // Notify DeviceChanged event for backward compatibility.
  // TODO(jennyz): remove this code when the old version of hotrod retires.
  NotifyDeviceChanged();
}

void AudioServiceImpl::NotifyMuteChanged(bool is_input, bool is_muted) {
  for (auto& observer : observer_list_)
    observer.OnMuteChanged(is_input, is_muted);

  // Notify DeviceChanged event for backward compatibility.
  // TODO(jennyz): remove this code when the old version of hotrod retires.
  NotifyDeviceChanged();
}

void AudioServiceImpl::NotifyDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  chromeos::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);

  DeviceInfoList device_info_list;
  for (const auto& device : devices) {
    device_info_list.push_back(ToAudioDeviceInfo(device));
  }

  for (auto& observer : observer_list_)
    observer.OnDevicesChanged(device_info_list);

  // Notify DeviceChanged event for backward compatibility.
  // TODO(jennyz): remove this code when the old version of hotrod retires.
  NotifyDeviceChanged();
}

AudioService* AudioService::CreateInstance(
    AudioDeviceIdCalculator* id_calculator) {
  return new AudioServiceImpl(id_calculator);
}

}  // namespace extensions
