// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_system_to_service_adapter.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

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

std::string ParamsToString(std::optional<AudioParameters> params) {
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
  NOTREACHED_IN_MIGRATION();
}

OnAudioParamsCallback WrapGetStreamParametersReply(
    StreamType stream_type,
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  const Action action = (stream_type == kInput) ? kGetInputStreamParameters
                                                : kGetOutputStreamParameters;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const char* name = GetTraceEvent(action);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
      "device id", device_id);

  return base::BindOnce(
      [](const char* name, base::TimeTicks start_time,
         OnAudioParamsCallback on_params_callback,
         const std::optional<media::AudioParameters>& params) {
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
            "params", ParamsToString(params));
        std::move(on_params_callback).Run(params);
      },
      name, start_time, std::move(on_params_callback));
}

OnBoolCallback WrapHasDevicesReply(StreamType stream_type,
                                   OnBoolCallback on_has_devices_callback) {
  const Action action =
      (stream_type == kInput) ? kHasInputDevices : kHasOutputDevices;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const char* name = GetTraceEvent(action);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)));

  return base::BindOnce(
      [](const char* name, base::TimeTicks start_time,
         OnBoolCallback on_has_devices_callback, bool answer) {
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
            "answer", answer);
        std::move(on_has_devices_callback).Run(answer);
      },
      name, start_time, std::move(on_has_devices_callback));
}

OnDeviceDescriptionsCallback WrapGetDeviceDescriptionsReply(
    StreamType stream_type,
    OnDeviceDescriptionsCallback on_descriptions_callback) {
  const Action action = (stream_type == kInput) ? kGetInputDeviceDescriptions
                                                : kGetOutputDeviceDescriptions;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const char* name = GetTraceEvent(action);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)));

  return base::BindOnce(
      [](const char* name, base::TimeTicks start_time,
         OnDeviceDescriptionsCallback on_descriptions_callback,
         media::AudioDeviceDescriptions descriptions) {
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
            "device count", descriptions.size());
        std::move(on_descriptions_callback).Run(std::move(descriptions));
      },
      name, start_time, std::move(on_descriptions_callback));
}

OnDeviceIdCallback WrapGetAssociatedOutputDeviceIDReply(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const char* name = GetTraceEvent(kGetAssociatedOutputDeviceID);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
      "input_device_id", input_device_id);

  return base::BindOnce(
      [](const char* name, base::TimeTicks start_time,
         OnDeviceIdCallback on_device_id_callback,
         const std::optional<std::string>& answer) {
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
            "answer", answer.value_or("nullopt"));
        std::move(on_device_id_callback).Run(answer);
      },
      name, start_time, std::move(on_device_id_callback));
}

OnInputDeviceInfoCallback WrapGetInputDeviceInfoReply(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const char* name = GetTraceEvent(kGetInputDeviceInfo);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
      "input_device_id", input_device_id);

  return base::BindOnce(
      [](const char* name, base::TimeTicks start_time,
         OnInputDeviceInfoCallback on_input_device_info_callback,
         const std::optional<AudioParameters>& params,
         const std::optional<std::string>& associated_output_device_id) {
        TRACE_EVENT_NESTABLE_ASYNC_END2(
            "audio", name, TRACE_ID_WITH_SCOPE(name, ToTraceId(start_time)),
            "params", ParamsToString(params), "associated_output_device_id",
            associated_output_device_id.value_or("nullopt"));
        std::move(on_input_device_info_callback)
            .Run(params, associated_output_device_id);
      },
      name, start_time, std::move(on_input_device_info_callback));
}

void ReportGetDeviceDescriptionResult(bool success) {
  base::UmaHistogramBoolean("Media.AudioSystem.GetDeviceDescription.Result",
                            success);
}

}  // namespace

AudioSystemToServiceAdapter::AudioSystemToServiceAdapter(
    SystemInfoBinder system_info_binder,
    base::TimeDelta disconnect_timeout)
    : system_info_binder_(std::move(system_info_binder)),
      disconnect_timeout_(disconnect_timeout) {
  DCHECK(system_info_binder_);
  DETACH_FROM_THREAD(thread_checker_);
}

AudioSystemToServiceAdapter::AudioSystemToServiceAdapter(
    SystemInfoBinder system_info_binder)
    : AudioSystemToServiceAdapter(std::move(system_info_binder),
                                  base::TimeDelta()) {}

AudioSystemToServiceAdapter::~AudioSystemToServiceAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (system_info_.is_bound()) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "audio", "AudioSystemToServiceAdapter bound", TRACE_ID_LOCAL(this),
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
                     std::nullopt));
}

void AudioSystemToServiceAdapter::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_callback) {
  GetSystemInfo()->GetOutputStreamParameters(
      device_id, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                     WrapGetStreamParametersReply(
                         kOutput, device_id, std::move(on_params_callback)),
                     std::nullopt));
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
  base::OnceCallback reporting_wrapped_callback = base::BindOnce(
      [](OnDeviceDescriptionsCallback cb, bool success,
         media::AudioDeviceDescriptions descriptions) {
        ReportGetDeviceDescriptionResult(success);
        WrapCallbackWithDeviceNameLocalization(std::move(cb))
            .Run(std::move(descriptions));
      },
      std::move(on_descriptions_callback));
  auto reply_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(reporting_wrapped_callback),
      /*success=*/false, media::AudioDeviceDescriptions());
  if (for_input)
    GetSystemInfo()->GetInputDeviceDescriptions(WrapGetDeviceDescriptionsReply(
        kInput, base::BindOnce(std::move(reply_callback), /*success=*/true)));
  else
    GetSystemInfo()->GetOutputDeviceDescriptions(WrapGetDeviceDescriptionsReply(
        kOutput, base::BindOnce(std::move(reply_callback), /*success=*/true)));
}

void AudioSystemToServiceAdapter::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_callback) {
  GetSystemInfo()->GetAssociatedOutputDeviceID(
      input_device_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          WrapGetAssociatedOutputDeviceIDReply(
              input_device_id, std::move(on_device_id_callback)),
          std::nullopt));
}

void AudioSystemToServiceAdapter::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_callback) {
  GetSystemInfo()->GetInputDeviceInfo(
      input_device_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          WrapGetInputDeviceInfoReply(input_device_id,
                                      std::move(on_input_device_info_callback)),
          std::nullopt, std::nullopt));
}

mojom::SystemInfo* AudioSystemToServiceAdapter::GetSystemInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!system_info_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "audio", "AudioSystemToServiceAdapter bound", TRACE_ID_LOCAL(this));
    system_info_binder_.Run(system_info_.BindNewPipeAndPassReceiver());
    system_info_.set_disconnect_handler(
        base::BindOnce(&AudioSystemToServiceAdapter::OnConnectionError,
                       base::Unretained(this)));
    if (!disconnect_timeout_.is_zero())
      system_info_.reset_on_idle_timeout(disconnect_timeout_);
  }

  return system_info_.get();
}

void AudioSystemToServiceAdapter::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "AudioSystemToServiceAdapter bound",
                                  TRACE_ID_LOCAL(this), "disconnect reason",
                                  "connection error");
  system_info_.reset();
}

}  // namespace audio
