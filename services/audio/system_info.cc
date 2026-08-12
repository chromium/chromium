// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/system_info.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "build/buildflag.h"
#include "media/base/audio_parameters.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
#include "media/webrtc/ml_model_handle.h"  // nogncheck crbug.com/40147906
#include "services/audio/ml_model_manager.h"
#endif

namespace audio {

namespace {

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
std::optional<media::AudioParameters> UpdateVoiceIsolationSupport(
    std::optional<media::AudioParameters> params,
    bool is_supported) {
  if (params) {
    int effects = params->effects();
    if (is_supported) {
      effects |= media::AudioParameters::VOICE_ISOLATION_SUPPORTED;
      params->set_effects(effects);
    } else {
      CHECK(!(effects & media::AudioParameters::VOICE_ISOLATION_SUPPORTED));
    }
  }
  return params;
}

bool IsVoiceIsolationSupported(MlModelManager* ml_model_manager) {
  return ml_model_manager &&
         ml_model_manager->GetModel(
             mojom::MlModelType::kVoiceIsolationDenoiser) != nullptr;
}
#endif

}  // namespace

SystemInfo::SystemInfo(media::AudioManager* audio_manager
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
                       ,
                       MlModelManager* ml_model_manager
#endif
                       )
    : helper_(audio_manager)
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
      ,
      ml_model_manager_(ml_model_manager)
#endif
{
}

SystemInfo::~SystemInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
}

void SystemInfo::Bind(mojo::PendingReceiver<mojom::SystemInfo> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void SystemInfo::GetInputStreamParameters(
    const std::string& device_id,
    GetInputStreamParametersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
  TRACE_EVENT1("audio", "audio::SystemInfo::GetInputStreamParameters",
               "device_id", device_id);
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  auto wrapped_callback = base::BindOnce(
      [](bool is_supported, GetInputStreamParametersCallback original_callback,
         const std::optional<media::AudioParameters>& params) {
        std::move(original_callback)
            .Run(UpdateVoiceIsolationSupport(params, is_supported));
      },
      IsVoiceIsolationSupported(ml_model_manager_), std::move(callback));

  helper_.GetInputStreamParameters(device_id, std::move(wrapped_callback));
#else
  helper_.GetInputStreamParameters(device_id, std::move(callback));
#endif
}

void SystemInfo::GetOutputStreamParameters(
    const std::string& device_id,
    GetOutputStreamParametersCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetOutputStreamParameters",
               "device_id", device_id);
  helper_.GetOutputStreamParameters(device_id, std::move(callback));
}

void SystemInfo::HasInputDevices(HasInputDevicesCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::HasInputDevices");
  helper_.HasInputDevices(std::move(callback));
}

void SystemInfo::HasOutputDevices(HasOutputDevicesCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::HasOutputDevices");
  helper_.HasOutputDevices(std::move(callback));
}

void SystemInfo::GetInputDeviceDescriptions(
    GetInputDeviceDescriptionsCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::GetInputDeviceDescriptions");
  helper_.GetDeviceDescriptions(true /* for_input */, std::move(callback));
}

void SystemInfo::GetOutputDeviceDescriptions(
    GetOutputDeviceDescriptionsCallback callback) {
  TRACE_EVENT0("audio", "audio::SystemInfo::GetOutputDeviceDescriptions");
  helper_.GetDeviceDescriptions(false /* for_input */, std::move(callback));
}

void SystemInfo::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    GetAssociatedOutputDeviceIDCallback callback) {
  TRACE_EVENT1("audio", "audio::SystemInfo::GetAssociatedOutputDeviceID",
               "input_device_id", input_device_id);
  helper_.GetAssociatedOutputDeviceID(input_device_id, std::move(callback));
}

void SystemInfo::GetInputDeviceInfo(const std::string& input_device_id,
                                    GetInputDeviceInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(binding_sequence_checker_);
  TRACE_EVENT1("audio", "audio::SystemInfo::GetInputDeviceInfo",
               "input_device_id", input_device_id);
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  auto wrapped_callback = base::BindOnce(
      [](bool is_supported, GetInputDeviceInfoCallback original_callback,
         const std::optional<media::AudioParameters>& input_params,
         const std::optional<std::string>& associated_output_device_id) {
        std::move(original_callback)
            .Run(UpdateVoiceIsolationSupport(input_params, is_supported),
                 associated_output_device_id);
      },
      IsVoiceIsolationSupported(ml_model_manager_), std::move(callback));

  helper_.GetInputDeviceInfo(input_device_id, std::move(wrapped_callback));
#else
  helper_.GetInputDeviceInfo(input_device_id, std::move(callback));
#endif
}

}  // namespace audio
