// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service_lacros.h"

#include "base/ranges/algorithm.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/api/audio/audio_service_utils.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

const char* const kServiceNotAvailableErrorMsg =
    "Audio devices crosapi interface is not available";

bool IsAudioCrosapiAvailable() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service);
  return service->IsAvailable<crosapi::mojom::AudioService>();
}

crosapi::mojom::AudioService& GetAudioCrosapi() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service);
  return *(service->GetRemote<crosapi::mojom::AudioService>());
}

}  // namespace

namespace extensions {

AudioServiceLacros::CrosapiObserver::CrosapiObserver() = default;
AudioServiceLacros::CrosapiObserver::~CrosapiObserver() = default;

void AudioServiceLacros::CrosapiObserver::OnDeviceListChanged(
    std::vector<crosapi::mojom::AudioDeviceInfoPtr> devices) {
  DeviceInfoList result;
  base::ranges::transform(devices, std::back_inserter(result),
                          extensions::ConvertAudioDeviceInfoFromMojom);
  for (auto& observer : observer_list_) {
    observer.OnDevicesChanged(result);
  }
}

void AudioServiceLacros::CrosapiObserver::OnLevelChanged(const std::string& id,
                                                         int32_t level) {
  for (auto& observer : observer_list_) {
    observer.OnLevelChanged(id, level);
  }
}

void AudioServiceLacros::CrosapiObserver::OnMuteChanged(bool is_input,
                                                        bool is_muted) {
  for (auto& observer : observer_list_) {
    observer.OnMuteChanged(is_input, is_muted);
  }
}

void AudioServiceLacros::CrosapiObserver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AudioServiceLacros::CrosapiObserver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AudioServiceLacros::AddObserver(Observer* observer) {
  crosapi_observer_.AddObserver(observer);
}

void AudioServiceLacros::RemoveObserver(Observer* observer) {
  crosapi_observer_.RemoveObserver(observer);
}

AudioServiceLacros::AudioServiceLacros() {
  if (IsAudioCrosapiAvailable()) {
    GetAudioCrosapi().AddAudioChangeObserver(
        receiver_.BindNewPipeAndPassRemote());
  } else {
    LOG(ERROR) << "Failed to add audio crosapi observer. "
               << kServiceNotAvailableErrorMsg;
  }
}

AudioServiceLacros::~AudioServiceLacros() = default;

void AudioServiceLacros::GetDevices(
    const api::audio::DeviceFilter* filter,
    base::OnceCallback<void(bool, DeviceInfoList)> extapi_callback) {
  if (!IsAudioCrosapiAvailable()) {
    LOG(ERROR) << "GetDevices. " << kServiceNotAvailableErrorMsg;
    DeviceInfoList devices_empty;
    std::move(extapi_callback).Run(false, std::move(devices_empty));
    return;
  }

  auto crosapi_callback = base::BindOnce(
      [](base::OnceCallback<void(bool, DeviceInfoList)> extapi_callback,
         absl::optional<std::vector<crosapi::mojom::AudioDeviceInfoPtr>>
             crosapi_devices) {
        bool result_out = false;
        DeviceInfoList devices_out;

        if (crosapi_devices) {
          result_out = true;
          base::ranges::transform(*crosapi_devices,
                                  std::back_inserter(devices_out),
                                  extensions::ConvertAudioDeviceInfoFromMojom);
        }

        std::move(extapi_callback).Run(result_out, std::move(devices_out));
      },
      std::move(extapi_callback));

  auto crosapi_filter = ConvertDeviceFilterToMojom(filter);

  GetAudioCrosapi().GetDevices(std::move(crosapi_filter),
                               std::move(crosapi_callback));
}

void AudioServiceLacros::SetActiveDeviceLists(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives,
    base::OnceCallback<void(bool)> callback) {
  if (!IsAudioCrosapiAvailable()) {
    LOG(ERROR) << "SetActiveDevices. " << kServiceNotAvailableErrorMsg;
    std::move(callback).Run(false);
    return;
  }

  auto ids = ConvertDeviceIdListsToMojom(input_devices, output_devives);
  GetAudioCrosapi().SetActiveDeviceLists(std::move(ids), std::move(callback));
}

void AudioServiceLacros::SetDeviceSoundLevel(
    const std::string& device_id,
    int level_value,
    base::OnceCallback<void(bool)> callback) {
  if (!IsAudioCrosapiAvailable()) {
    LOG(ERROR) << "SetDeviceSoundLevel. " << kServiceNotAvailableErrorMsg;
    std::move(callback).Run(false);
    return;
  }

  auto properties = crosapi::mojom::AudioDeviceProperties::New(level_value);
  GetAudioCrosapi().SetProperties(device_id, std::move(properties),
                                  std::move(callback));
}

void AudioServiceLacros::SetMute(bool is_input,
                                 bool value,
                                 base::OnceCallback<void(bool)> callback) {
  if (!IsAudioCrosapiAvailable()) {
    LOG(ERROR) << "SetMute. " << kServiceNotAvailableErrorMsg;
    std::move(callback).Run(false);
    return;
  }

  crosapi::mojom::StreamType stream_type =
      is_input ? crosapi::mojom::StreamType::kInput
               : crosapi::mojom::StreamType::kOutput;
  GetAudioCrosapi().SetMute(stream_type, value, std::move(callback));
}

void AudioServiceLacros::GetMute(
    bool is_input,
    base::OnceCallback<void(bool, bool)> callback) {
  if (!IsAudioCrosapiAvailable()) {
    LOG(ERROR) << "GetMute. " << kServiceNotAvailableErrorMsg;
    std::move(callback).Run(false, false);
    return;
  }

  crosapi::mojom::StreamType stream_type =
      is_input ? crosapi::mojom::StreamType::kInput
               : crosapi::mojom::StreamType::kOutput;
  GetAudioCrosapi().GetMute(stream_type, std::move(callback));
}

AudioService::Ptr AudioService::CreateInstance(
    AudioDeviceIdCalculator* id_calculator) {
  return std::make_unique<AudioServiceLacros>();
}

}  // namespace extensions
