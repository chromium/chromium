// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"

namespace extensions {

using api::audio::AudioDeviceInfo;
using ::ash::AudioDevice;
using ::ash::AudioDeviceType;
using ::ash::CrasAudioHandler;
using ::content::BrowserThread;

namespace {

api::audio::DeviceType GetAsAudioApiDeviceType(AudioDeviceType type) {
  switch (type) {
    case AudioDeviceType::kHeadphone:
      return api::audio::DeviceType::kHeadphone;
    case AudioDeviceType::kMic:
      return api::audio::DeviceType::kMic;
    case AudioDeviceType::kUsb:
      return api::audio::DeviceType::kUsb;
    case AudioDeviceType::kBluetooth:
    case AudioDeviceType::kBluetoothNbMic:
      return api::audio::DeviceType::kBluetooth;
    case AudioDeviceType::kHdmi:
      return api::audio::DeviceType::kHdmi;
    case AudioDeviceType::kInternalSpeaker:
      return api::audio::DeviceType::kInternalSpeaker;
    case AudioDeviceType::kInternalMic:
      return api::audio::DeviceType::kInternalMic;
    case AudioDeviceType::kFrontMic:
      return api::audio::DeviceType::kFrontMic;
    case AudioDeviceType::kRearMic:
      return api::audio::DeviceType::kRearMic;
    case AudioDeviceType::kKeyboardMic:
      return api::audio::DeviceType::kKeyboardMic;
    case AudioDeviceType::kHotword:
      return api::audio::DeviceType::kHotword;
    case AudioDeviceType::kLineout:
      return api::audio::DeviceType::kLineout;
    case AudioDeviceType::kPostMixLoopback:
      return api::audio::DeviceType::kPostMixLoopback;
    case AudioDeviceType::kPostDspLoopback:
      return api::audio::DeviceType::kPostDspLoopback;
    case AudioDeviceType::kAlsaLoopback:
      return api::audio::DeviceType::kAlsaLoopback;
    case AudioDeviceType::kOther:
      return api::audio::DeviceType::kOther;
  }

  NOTREACHED_IN_MIGRATION();
  return api::audio::DeviceType::kOther;
}

}  // namespace

class AudioServiceImpl : public AudioService,
                         public CrasAudioHandler::AudioObserver {
 public:
  explicit AudioServiceImpl(AudioDeviceIdCalculator* id_calculator);

  AudioServiceImpl(const AudioServiceImpl&) = delete;
  AudioServiceImpl& operator=(const AudioServiceImpl&) = delete;

  ~AudioServiceImpl() override;

  // Called by listeners to this service to add/remove themselves as observers.
  void AddObserver(AudioService::Observer* observer) override;
  void RemoveObserver(AudioService::Observer* observer) override;

  // Start to query audio device information.
  void GetDevices(
      const api::audio::DeviceFilter* filter,
      base::OnceCallback<void(bool, DeviceInfoList)> callback) override;
  void SetActiveDeviceLists(const DeviceIdList* input_devices,
                            const DeviceIdList* output_devives,
                            base::OnceCallback<void(bool)> callback) override;
  void SetDeviceSoundLevel(const std::string& device_id,
                           int level_value,
                           base::OnceCallback<void(bool)> callback) override;
  void SetMute(bool is_input,
               bool value,
               base::OnceCallback<void(bool)> callback) override;
  void GetMute(bool is_input,
               base::OnceCallback<void(bool, bool)> callback) override;

 protected:
  // CrasAudioHandler::AudioObserver overrides.
  void OnOutputNodeVolumeChanged(uint64_t id, int volume) override;
  void OnInputNodeGainChanged(uint64_t id, int gain) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

 private:
  void NotifyLevelChanged(uint64_t id, int level);
  void NotifyMuteChanged(bool is_input, bool is_muted);
  void NotifyDevicesChanged();

  uint64_t GetIdFromStr(const std::string& id_str);
  bool GetAudioNodeIdList(const DeviceIdList& ids,
                          bool is_input,
                          CrasAudioHandler::NodeIdList* node_ids);
  AudioDeviceInfo ToAudioDeviceInfo(const AudioDevice& device);

  // List of observers.
  base::ObserverList<AudioService::Observer>::Unchecked observer_list_;

  raw_ptr<CrasAudioHandler, DanglingUntriaged> cras_audio_handler_;

  raw_ptr<AudioDeviceIdCalculator> id_calculator_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AudioServiceImpl> weak_ptr_factory_{this};
};

AudioServiceImpl::AudioServiceImpl(AudioDeviceIdCalculator* id_calculator)
    : cras_audio_handler_(CrasAudioHandler::Get()),
      id_calculator_(id_calculator) {
  CHECK(id_calculator_);

  if (cras_audio_handler_)
    cras_audio_handler_->AddAudioObserver(this);
}

AudioServiceImpl::~AudioServiceImpl() {
  // The CrasAudioHandler global instance may have already been destroyed, so
  // do not used the cached pointer here.
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void AudioServiceImpl::AddObserver(AudioService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AudioServiceImpl::RemoveObserver(AudioService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AudioServiceImpl::GetDevices(
    const api::audio::DeviceFilter* filter,
    base::OnceCallback<void(bool, DeviceInfoList)> callback) {
  DeviceInfoList devices_out;
  if (!cras_audio_handler_) {
    std::move(callback).Run(false, std::move(devices_out));
    return;
  }

  ash::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);

  bool accept_input =
      !(filter && filter->stream_types) ||
      base::Contains(*filter->stream_types, api::audio::StreamType::kInput);
  bool accept_output =
      !(filter && filter->stream_types) ||
      base::Contains(*filter->stream_types, api::audio::StreamType::kOutput);

  for (const auto& device : devices) {
    if (filter && filter->is_active && *filter->is_active != device.active)
      continue;
    if (device.is_input && !accept_input)
      continue;
    if (!device.is_input && !accept_output)
      continue;
    devices_out.push_back(ToAudioDeviceInfo(device));
  }

  std::move(callback).Run(true, std::move(devices_out));
}

void AudioServiceImpl::SetActiveDeviceLists(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_) {
    std::move(callback).Run(false);
    return;
  }

  CrasAudioHandler::NodeIdList input_nodes;
  if (input_devices &&
      !GetAudioNodeIdList(*input_devices, true, &input_nodes)) {
    std::move(callback).Run(false);
    return;
  }

  CrasAudioHandler::NodeIdList output_nodes;
  if (output_devives &&
      !GetAudioNodeIdList(*output_devives, false, &output_nodes)) {
    std::move(callback).Run(false);
    return;
  }

  bool success = true;
  if (output_devives) {
    success = cras_audio_handler_->SetActiveOutputNodes(output_nodes);
    DCHECK(success);
  }

  if (input_devices) {
    success = success && cras_audio_handler_->SetActiveInputNodes(input_nodes);
    DCHECK(success);
  }
  std::move(callback).Run(success);
}

void AudioServiceImpl::SetDeviceSoundLevel(
    const std::string& device_id,
    int level_value,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_) {
    std::move(callback).Run(false);
    return;
  }

  const AudioDevice* device =
      cras_audio_handler_->GetDeviceFromId(GetIdFromStr(device_id));
  if (!device) {
    std::move(callback).Run(false);
    return;
  }

  if (level_value != -1) {
    cras_audio_handler_->SetVolumeGainPercentForDevice(device->id, level_value);
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void AudioServiceImpl::SetMute(bool is_input,
                               bool value,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_) {
    std::move(callback).Run(false);
    return;
  }

  if (is_input)
    cras_audio_handler_->SetInputMute(
        value, CrasAudioHandler::InputMuteChangeMethod::kOther);
  else
    cras_audio_handler_->SetOutputMute(value);

  std::move(callback).Run(true);
}

void AudioServiceImpl::GetMute(bool is_input,
                               base::OnceCallback<void(bool, bool)> callback) {
  DCHECK(cras_audio_handler_);
  if (!cras_audio_handler_) {
    std::move(callback).Run(false, false);
    return;
  }

  const bool is_muted_result = is_input ? cras_audio_handler_->IsInputMuted()
                                        : cras_audio_handler_->IsOutputMuted();
  std::move(callback).Run(true, is_muted_result);
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
    CrasAudioHandler::NodeIdList* node_ids) {
  for (const auto& device_id : ids) {
    const AudioDevice* device =
        cras_audio_handler_->GetDeviceFromId(GetIdFromStr(device_id));
    if (!device)
      return false;
    if (device->is_input != is_input)
      return false;
    node_ids->push_back(device->id);
  }
  return true;
}

AudioDeviceInfo AudioServiceImpl::ToAudioDeviceInfo(const AudioDevice& device) {
  AudioDeviceInfo info;
  info.id = base::NumberToString(device.id);
  info.stream_type = device.is_input
                         ? extensions::api::audio::StreamType::kInput
                         : extensions::api::audio::StreamType::kOutput;
  info.device_type = GetAsAudioApiDeviceType(device.type);
  info.display_name = device.display_name;
  info.device_name = device.device_name;
  info.is_active = device.active;
  info.level =
      device.is_input
          ? cras_audio_handler_->GetInputGainPercentForDevice(device.id)
          : cras_audio_handler_->GetOutputVolumePercentForDevice(device.id);
  info.stable_device_id =
      id_calculator_->GetStableDeviceId(device.stable_device_id);

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

void AudioServiceImpl::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  NotifyMuteChanged(true, mute_on);
}

void AudioServiceImpl::OnAudioNodesChanged() {
  NotifyDevicesChanged();
}

void AudioServiceImpl::OnActiveOutputNodeChanged() {}

void AudioServiceImpl::OnActiveInputNodeChanged() {}

void AudioServiceImpl::NotifyLevelChanged(uint64_t id, int level) {
  for (auto& observer : observer_list_)
    observer.OnLevelChanged(base::NumberToString(id), level);
}

void AudioServiceImpl::NotifyMuteChanged(bool is_input, bool is_muted) {
  for (auto& observer : observer_list_)
    observer.OnMuteChanged(is_input, is_muted);
}

void AudioServiceImpl::NotifyDevicesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ash::AudioDeviceList devices;
  cras_audio_handler_->GetAudioDevices(&devices);

  DeviceInfoList device_info_list;
  for (const auto& device : devices) {
    device_info_list.push_back(ToAudioDeviceInfo(device));
  }

  for (auto& observer : observer_list_)
    observer.OnDevicesChanged(device_info_list);
}

AudioService::Ptr AudioService::CreateInstance(
    AudioDeviceIdCalculator* id_calculator) {
  return std::make_unique<AudioServiceImpl>(id_calculator);
}

}  // namespace extensions
