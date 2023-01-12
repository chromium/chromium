// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/audio_system.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/system_info.mojom.h"

namespace audio {

// Provides media::AudioSystem implementation on top of Audio service.
// In case connection to Audio service is lost, reply callbacks will run with
// empty optionals / false booleans.
class COMPONENT_EXPORT(AUDIO_PUBLIC_CPP) AudioSystemToServiceAdapter
    : public media::AudioSystem {
 public:
  // A callback which can be used to acquire a new SystemInfo interface pipe
  // lazily as needed.
  using SystemInfoBinder =
      base::RepeatingCallback<void(mojo::PendingReceiver<mojom::SystemInfo>)>;

  // If |disconnect_timeout| is positive, the instance will disconnect from
  // Audio service upon |disconnect_timeout| if not in use, and reconnect on the
  // next attempt to use it.
  AudioSystemToServiceAdapter(SystemInfoBinder system_info_binder,
                              base::TimeDelta disconnect_timeout);
  explicit AudioSystemToServiceAdapter(SystemInfoBinder system_info_binder);

  AudioSystemToServiceAdapter(const AudioSystemToServiceAdapter&) = delete;
  AudioSystemToServiceAdapter& operator=(const AudioSystemToServiceAdapter&) =
      delete;

  ~AudioSystemToServiceAdapter() override;

  // AudioSystem implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_callback) override;
  void GetOutputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_callback) override;
  void HasInputDevices(OnBoolCallback on_has_devices_callback) override;
  void HasOutputDevices(OnBoolCallback on_has_devices_callback) override;
  void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cp) override;
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_callback) override;
  void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_callback) override;

 private:
  mojom::SystemInfo* GetSystemInfo();
  void OnConnectionError();

  // Will be bound to the thread AudioSystemToServiceAdapter is used on.
  const SystemInfoBinder system_info_binder_;
  mojo::Remote<mojom::SystemInfo> system_info_;

  // To disconnect from the audio service when not in use.
  const base::TimeDelta disconnect_timeout_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_SYSTEM_TO_SERVICE_ADAPTER_H_
