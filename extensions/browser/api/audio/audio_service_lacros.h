// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_LACROS_H_
#define EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_LACROS_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/audio_service.mojom.h"
#include "extensions/browser/api/audio/audio_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace extensions {

class AudioServiceLacros : public AudioService {
 public:
  using DeviceInfoList = std::vector<api::audio::AudioDeviceInfo>;

  AudioServiceLacros();
  ~AudioServiceLacros() override;

  AudioServiceLacros(const AudioServiceLacros&) = delete;
  AudioServiceLacros& operator=(const AudioServiceLacros&) = delete;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

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

 private:
  class CrosapiObserver : public crosapi::mojom::AudioChangeObserver {
   public:
    CrosapiObserver();
    ~CrosapiObserver() override;

    // crosapi::mojom::AudioChangeObserver implementation:
    void OnDeviceListChanged(
        std::vector<crosapi::mojom::AudioDeviceInfoPtr> devices) override;
    void OnLevelChanged(const std::string& id, int32_t level) override;
    void OnMuteChanged(bool is_input, bool is_muted) override;

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

   private:
    base::ObserverList<AudioService::Observer>::Unchecked observer_list_;
  };

  CrosapiObserver crosapi_observer_;
  mojo::Receiver<crosapi::mojom::AudioChangeObserver> receiver_{
      &crosapi_observer_};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_AUDIO_SERVICE_LACROS_H_
