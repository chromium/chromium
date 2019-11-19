// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_system_to_service_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

namespace {

using OnAudioParamsCallback = media::AudioSystem::OnAudioParamsCallback;
using OnDeviceIdCallback = media::AudioSystem::OnDeviceIdCallback;
using OnInputDeviceInfoCallback = media::AudioSystem::OnInputDeviceInfoCallback;
using OnBoolCallback = media::AudioSystem::OnBoolCallback;
using OnDeviceDescriptionsCallback =
    media::AudioSystem::OnDeviceDescriptionsCallback;
using media::AudioParameters;

int64_t ToTraceId(base::TimeTicks time) {
  return (time - base::TimeTicks()).InNanoseconds();
}

std::string ParamsToString(base::Optional<AudioParameters> params) {
  return params ? params->AsHumanReadableString() : "nullopt";
}

enum Action {
  kGetInputStreamParameters,
  kGetOutputStreamParameters,
  kHasInputDevices,
  kHasOutputDevices,
  kGetInputDeviceDescriptions,
  kGetOutputDeviceDescriptions,
  kGetAssociatedOutputDeviceID,
  kGetInputDeviceInfo
};

enum StreamType { kInput, kOutput };

const char* GetTraceEvent(Action action) {
  switch (action) {
    case kGetInputStreamParameters:
      return "AudioSystemToServiceAdapter::GetInputStreamParameters";
    case kGetOutputStreamParameters:
      return "AudioSystemToServiceAdapter::GetOutputStreamParameters";
    case kHasInputDevices:
      return "AudioSystemToServiceAdapter::HasInputDevices";
    case kHasOutputDevices:
      return "AudioSystemToServiceAdapter::HasOutputDevices";
    case kGetInputDeviceDescriptions:
      return "AudioSystemToServiceAdapter::GetInputDeviceDescriptions";
    case kGetOutputDeviceDescriptions:
      return "AudioSystemToServiceAdapter::GetOutputDeviceDescriptions";
    case kGetAssociatedOutputDeviceID:
      return "AudioSystemToServiceAdapter::GetAssociatedOutputDeviceID";
    case kGetInputDeviceInfo:
      return "AudioSystemToServiceAdapter::GetInputDeviceInfo";
  }
  NOTREACHED();
}

void LogUMA(Action action, base::TimeTicks start_time) {
  const base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  switch (action) {
    case kGetInputStreamParameters:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetInputStreamParameters",
          duration);
      return;
    case kGetOutputStreamParameters:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetOutputStreamParameters",
          duration);
      return;
    case kHasInputDevices:
      UMA_HISTOGRAM_TIMES("Media.AudioService.SystemInfoClient.HasInputDevices",
                          duration);
      return;
    case kHasOutputDevices:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.HasOutputDevices", duration);
      return;
    case kGetInputDeviceDescriptions:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetInputDeviceDescriptions",
          duration);
      return;
    case kGetOutputDeviceDescriptions:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetOutputDeviceDescriptions",
          duration);
      return;
    case kGetAssociatedOutputDeviceID:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetAssociatedOutputDeviceID",
          duration);
      return;
    case kGetInputDeviceInfo:
      UMA_HISTOGRAM_TIMES(
          "Media.AudioService.SystemInfoClient.GetInputDeviceInfo", duration);
      return;
  }
  NOTREACHED();
}

OnAudioParamsCallback WrapGetStreamParametersReply(
    StreamType stream_type,
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  const Action action = (stream_type == kInput) ? kGetInputStreamParameters
                                                : kGetOutputStreamParameters;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_ASYNC_BEGIN1("audio", GetTraceEvent(action),
                           ToTraceId(start_time), "device id", device_id);

  return base::BindOnce(
      [](Action action, base::TimeTicks start_time,
         OnAudioParamsCallback on_params_callback,
         const base::Optional<media::AudioParameters>& params) {
        TRACE_EVENT_ASYNC_END1("audio", GetTraceEvent(action),
                               ToTraceId(start_time), "params",
                               ParamsToString(params));
        LogUMA(action, start_time);
        std::move(on_params_callback).Run(params);
      },
      action, start_time, std::move(on_params_callback));
}

OnBoolCallback WrapHasDevicesReply(StreamType stream_type,
                                   OnBoolCallback on_has_devices_callback) {
  const Action action =
      (stream_type == kInput) ? kHasInputDevices : kHasOutputDevices;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_ASYNC_BEGIN0("audio", GetTraceEvent(action),
                           ToTraceId(start_time));

  return base::BindOnce(
      [](Action action, base::TimeTicks start_time,
         OnBoolCallback on_has_devices_callback, bool answer) {
        TRACE_EVENT_ASYNC_END1("audio", GetTraceEvent(action),
                               ToTraceId(start_time), "answer", answer);
        LogUMA(action, start_time);
        std::move(on_has_devices_callback).Run(answer);
      },
      action, start_time, std::move(on_has_devices_callback));
}

OnDeviceDescriptionsCallback WrapGetDeviceDescriptionsReply(
    StreamType stream_type,
    OnDeviceDescriptionsCallback on_descriptions_callback) {
  const Action action = (stream_type == kInput) ? kGetInputDeviceDescriptions
                                                : kGetOutputDeviceDescriptions;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_ASYNC_BEGIN0("audio", GetTraceEvent(action),
                           ToTraceId(start_time));

  return base::BindOnce(
      [](Action action, base::TimeTicks start_time,
         OnDeviceDescriptionsCallback on_descriptions_callback,
         media::AudioDeviceDescriptions descriptions) {
        TRACE_EVENT_ASYNC_END1("audio", GetTraceEvent(action),
                               ToTraceId(start_time), "device count",
                               descriptions.size());
        LogUMA(action, start_time);
        std::move(on_descriptions_callback).Run(std::move(descriptions));
      },
      action, start_time, std::move(on_descriptions_callback));
}

OnDeviceIdCallback WrapGetAssociatedOutputDeviceIDReply(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_ASYNC_BEGIN1("audio", GetTraceEvent(kGetAssociatedOutputDeviceID),
                           ToTraceId(start_time), "input_device_id",
                           input_device_id);

  return base::BindOnce(
      [](base::TimeTicks start_time, OnDeviceIdCallback on_device_id_callback,
         const base::Optional<std::string>& answer) {
        TRACE_EVENT_ASYNC_END1(
            "audio", GetTraceEvent(kGetAssociatedOutputDeviceID),
            ToTraceId(start_time), "answer", answer.value_or("nullopt"));
        LogUMA(kGetAssociatedOutputDeviceID, start_time);
        std::move(on_device_id_callback).Run(answer);
      },
      start_time, std::move(on_device_id_callback));
}

OnInputDeviceInfoCallback WrapGetInputDeviceInfoReply(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  TRACE_EVENT_ASYNC_BEGIN1("audio", GetTraceEvent(kGetInputDeviceInfo),
                           ToTraceId(start_time), "input_device_id",
                           input_device_id);

  return base::BindOnce(
      [](base::TimeTicks start_time,
         OnInputDeviceInfoCallback on_input_device_info_callback,
         const base::Optional<AudioParameters>& params,
         const base::Optional<std::string>& associated_output_device_id) {
        TRACE_EVENT_ASYNC_END2(
            "audio", GetTraceEvent(kGetInputDeviceInfo), ToTraceId(start_time),
            "params", ParamsToString(params), "associated_output_device_id",
            associated_output_device_id.value_or("nullopt"));
        LogUMA(kGetInputDeviceInfo, start_time);
        std::move(on_input_device_info_callback)
            .Run(params, associated_output_device_id);
      },
      start_time, std::move(on_input_device_info_callback));
}

}  // namespace

AudioSystemToServiceAdapter::AudioSystemToServiceAdapter(
    std::unique_ptr<service_manager::Connector> connector,
    base::TimeDelta disconnect_timeout)
    : connector_(std::move(connector)),
      disconnect_timeout_(disconnect_timeout) {
  DCHECK(connector_);
  DETACH_FROM_THREAD(thread_checker_);
}

AudioSystemToServiceAdapter::AudioSystemToServiceAdapter(
    std::unique_ptr<service_manager::Connector> connector)
    : AudioSystemToServiceAdapter(std::move(connector), base::TimeDelta()) {}

AudioSystemToServiceAdapter::~AudioSystemToServiceAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!!system_info_) {
    TRACE_EVENT_NESTABLE_ASYNC_END1("audio",
                                    "AudioSystemToServiceAdapter bound", this,
                                    "disconnect reason", "destroyed");
  }
}

void AudioSystemToServiceAdapter::GetInputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  GetSystemInfo()->GetInputStreamParameters(
      device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     WrapGetStreamParametersReply(
                         kInput, device_id, std::move(on_params_callback)),
                     base::nullopt));
}

void AudioSystemToServiceAdapter::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  GetSystemInfo()->GetOutputStreamParameters(
      device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     WrapGetStreamParametersReply(
                         kOutput, device_id, std::move(on_params_callback)),
                     base::nullopt));
}

void AudioSystemToServiceAdapter::HasInputDevices(
    OnBoolCallback on_has_devices_callback) {
  GetSystemInfo()->HasInputDevices(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      WrapHasDevicesReply(kInput, std::move(on_has_devices_callback)), false));
}

void AudioSystemToServiceAdapter::HasOutputDevices(
    OnBoolCallback on_has_devices_callback) {
  GetSystemInfo()->HasOutputDevices(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      WrapHasDevicesReply(kOutput, std::move(on_has_devices_callback)), false));
}

void AudioSystemToServiceAdapter::GetDeviceDescriptions(
    bool for_input,
    OnDeviceDescriptionsCallback on_descriptions_callback) {
  auto reply_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      WrapCallbackWithDeviceNameLocalization(
          std::move(on_descriptions_callback)),
      media::AudioDeviceDescriptions());
  if (for_input)
    GetSystemInfo()->GetInputDeviceDescriptions(
        WrapGetDeviceDescriptionsReply(kInput, std::move(reply_callback)));
  else
    GetSystemInfo()->GetOutputDeviceDescriptions(
        WrapGetDeviceDescriptionsReply(kOutput, std::move(reply_callback)));
}

void AudioSystemToServiceAdapter::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_callback) {
  GetSystemInfo()->GetAssociatedOutputDeviceID(
      input_device_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          WrapGetAssociatedOutputDeviceIDReply(
              input_device_id, std::move(on_device_id_callback)),
          base::nullopt));
}

void AudioSystemToServiceAdapter::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_callback) {
  GetSystemInfo()->GetInputDeviceInfo(
      input_device_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          WrapGetInputDeviceInfoReply(input_device_id,
                                      std::move(on_input_device_info_callback)),
          base::nullopt, base::nullopt));
}

mojom::SystemInfo* AudioSystemToServiceAdapter::GetSystemInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!system_info_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "audio", "AudioSystemToServiceAdapter bound", this);
    connector_->Connect(mojom::kServiceName,
                        system_info_.BindNewPipeAndPassReceiver());
    system_info_.set_disconnect_handler(
        base::BindOnce(&AudioSystemToServiceAdapter::OnConnectionError,
                       base::Unretained(this)));
    if (!disconnect_timeout_.is_zero())
      system_info_.reset_on_idle_timeout(disconnect_timeout_);
    DCHECK(system_info_);
  }

  return system_info_.get();
}

void AudioSystemToServiceAdapter::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "AudioSystemToServiceAdapter bound",
                                  this, "disconnect reason",
                                  "connection error");
  system_info_.reset();
}

}  // namespace audio
