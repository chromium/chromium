// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_system.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"

namespace content {

namespace {

// Returns (by callback) the Media Device salt and the Origin for the frame and
// whether it may request nondefault audio devices.
void CheckAccessOnUIThread(
    int render_process_id,
    int render_frame_id,
    bool override_permissions,
    bool permissions_override_value,
    base::OnceCallback<void(std::string, url::Origin, bool)> cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaDeviceSaltAndOrigin salt_and_origin =
      GetMediaDeviceSaltAndOrigin(render_process_id, render_frame_id);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id,
                                           salt_and_origin.origin)) {
    // In this case, it's likely a navigation has occurred while processing this
    // request.
    std::move(cb).Run(std::string(), url::Origin(), false);
    return;
  }

  // Check that MediaStream device permissions have been granted for
  // nondefault devices.
  if (override_permissions) {
    std::move(cb).Run(std::move(salt_and_origin.device_id_salt),
                      std::move(salt_and_origin.origin),
                      permissions_override_value);
    return;
  }

  std::move(cb).Run(std::move(salt_and_origin.device_id_salt),
                    std::move(salt_and_origin.origin),
                    MediaDevicesPermissionChecker().CheckPermissionOnUIThread(
                        blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT,
                        render_process_id, render_frame_id));
}

}  // namespace

class AudioOutputAuthorizationHandler::TraceScope {
 public:
  explicit TraceScope(const std::string& device_id) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "audio", "Audio output device authorization", this);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("audio", "Request for device", this,
                                      "device id", device_id);
  }

  ~TraceScope() {
    if (waiting_for_params_) {
      TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "Getting audio parameters", this,
                                      "cancelled", true);
    }
    if (checking_access_) {
      TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "Checking access", this,
                                      "cancelled", true);
    }
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Request for device", this);
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio",
                                    "Audio output device authorization", this);
  }

  void SimpleEvent(const char* event) {
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", event, this);
  }

  void UsingSessionId(const base::UnguessableToken& session_id,
                      const std::string& device_id) {
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT2("audio", "Using session id", this,
                                        "session id", session_id.ToString(),
                                        "device id", device_id);
  }

  void CheckAccessStart(const std::string& device_id) {
    checking_access_ = true;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("audio", "Checking access", this,
                                      "device id", device_id);
  }

  void AccessChecked(bool has_access) {
    checking_access_ = false;
    TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "Checking access", this,
                                    "access granted", has_access);
  }

  void StartedGettingAudioParameters(const std::string& raw_device_id) {
    waiting_for_params_ = true;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("audio", "Getting audio parameters", this,
                                      "device id", raw_device_id);
  }

  void FinishedGettingAudioParameters() {
    waiting_for_params_ = false;
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Getting audio parameters", this);
  }

 private:
  bool checking_access_ = false;
  bool waiting_for_params_ = false;

  DISALLOW_COPY_AND_ASSIGN(TraceScope);
};

AudioOutputAuthorizationHandler::AudioOutputAuthorizationHandler(
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager,
    int render_process_id)
    : audio_system_(audio_system),
      media_stream_manager_(media_stream_manager),
      render_process_id_(render_process_id) {
  DCHECK(media_stream_manager_);
}

AudioOutputAuthorizationHandler::~AudioOutputAuthorizationHandler() {
  // |weak_factory| is not thread safe. Make sure it's destructed on the
  // right thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void AudioOutputAuthorizationHandler::RequestDeviceAuthorization(
    int render_frame_id,
    const base::UnguessableToken& session_id,
    const std::string& device_id,
    AuthorizationCompletedCallback cb) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto trace_scope = std::make_unique<TraceScope>(device_id);

  if (!IsValidDeviceId(device_id)) {
    trace_scope->SimpleEvent("Invalid device id");
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string(), std::string());
    return;
  }

  // If |session_id| should be used for output device selection and such an
  // output device is found, reuse the input device permissions.
  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    const blink::MediaStreamDevice* device =
        media_stream_manager_->audio_input_device_manager()
            ->GetOpenedDeviceById(session_id);
    if (device && device->matched_output_device_id) {
      trace_scope->UsingSessionId(session_id, device->id);
      // We don't need the origin for authorization in this case, but it's used
      // for hashing the device id before sending it back to the renderer.
      base::PostTaskAndReplyWithResult(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&GetMediaDeviceSaltAndOrigin, render_process_id_,
                         render_frame_id),
          base::BindOnce(&AudioOutputAuthorizationHandler::HashDeviceId,
                         weak_factory_.GetWeakPtr(), std::move(trace_scope),
                         std::move(cb), *device->matched_output_device_id));
      return;
    }
    // Otherwise, the default device is used.
  }

  if (media::AudioDeviceDescription::IsDefaultDevice(device_id)) {
    // The default device doesn't need authorization.
    GetDeviceParameters(std::move(trace_scope), std::move(cb),
                        media::AudioDeviceDescription::kDefaultDeviceId);
    return;
  }

  trace_scope->CheckAccessStart(device_id);
  // Check device permissions if nondefault device is requested.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CheckAccessOnUIThread, render_process_id_,
                     render_frame_id, override_permissions_,
                     permissions_override_value_,
                     media::BindToCurrentLoop(base::BindOnce(
                         &AudioOutputAuthorizationHandler::AccessChecked,
                         weak_factory_.GetWeakPtr(), std::move(trace_scope),
                         std::move(cb), device_id))));
}

void AudioOutputAuthorizationHandler::OverridePermissionsForTesting(
    bool override_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  override_permissions_ = true;
  permissions_override_value_ = override_value;
}

void AudioOutputAuthorizationHandler::UMALogDeviceAuthorizationTime(
    base::TimeTicks auth_start_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.OutputDeviceAuthorizationTime",
                             base::TimeTicks::Now() - auth_start_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(5000), 50);
}

void AudioOutputAuthorizationHandler::HashDeviceId(
    std::unique_ptr<TraceScope> trace_scope,
    AuthorizationCompletedCallback cb,
    const std::string& raw_device_id,
    const MediaDeviceSaltAndOrigin& salt_and_origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());
  std::string hashed_device_id = GetHMACForMediaDeviceID(
      salt_and_origin.device_id_salt, salt_and_origin.origin, raw_device_id);
  trace_scope->StartedGettingAudioParameters(raw_device_id);
  audio_system_->GetOutputStreamParameters(
      raw_device_id,
      base::BindOnce(&AudioOutputAuthorizationHandler::DeviceParametersReceived,
                     weak_factory_.GetWeakPtr(), std::move(trace_scope),
                     std::move(cb), hashed_device_id, raw_device_id));
}

void AudioOutputAuthorizationHandler::AccessChecked(
    std::unique_ptr<TraceScope> trace_scope,
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    std::string salt,
    url::Origin security_origin,
    bool has_access) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  trace_scope->AccessChecked(has_access);

  if (!has_access) {
    std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
                      media::AudioParameters::UnavailableDeviceParams(),
                      std::string(), std::string());
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&AudioOutputAuthorizationHandler::TranslateDeviceID,
                     weak_factory_.GetWeakPtr(), std::move(trace_scope),
                     std::move(cb), device_id, std::move(salt),
                     std::move(security_origin)));
}

void AudioOutputAuthorizationHandler::TranslateDeviceID(
    std::unique_ptr<TraceScope> trace_scope,
    AuthorizationCompletedCallback cb,
    const std::string& device_id,
    const std::string& salt,
    const url::Origin& security_origin,
    const MediaDeviceEnumeration& enumeration) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!media::AudioDeviceDescription::IsDefaultDevice(device_id));

  for (const blink::WebMediaDeviceInfo& device_info :
       enumeration[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) {
    if (DoesMediaDeviceIDMatchHMAC(salt, security_origin, device_id,
                                   device_info.device_id)) {
      GetDeviceParameters(std::move(trace_scope), std::move(cb),
                          device_info.device_id);
      return;
    }
  }

  trace_scope->SimpleEvent("Found no device matching device id");
  std::move(cb).Run(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND,
                    media::AudioParameters::UnavailableDeviceParams(),
                    std::string(), std::string());
}

void AudioOutputAuthorizationHandler::GetDeviceParameters(
    std::unique_ptr<TraceScope> trace_scope,
    AuthorizationCompletedCallback cb,
    const std::string& raw_device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());

  trace_scope->StartedGettingAudioParameters(raw_device_id);
  audio_system_->GetOutputStreamParameters(
      raw_device_id,
      base::BindOnce(&AudioOutputAuthorizationHandler::DeviceParametersReceived,
                     weak_factory_.GetWeakPtr(), std::move(trace_scope),
                     std::move(cb), std::string(), raw_device_id));
}

void AudioOutputAuthorizationHandler::DeviceParametersReceived(
    std::unique_ptr<TraceScope> trace_scope,
    AuthorizationCompletedCallback cb,
    const std::string& id_for_renderer,
    const std::string& raw_device_id,
    const base::Optional<media::AudioParameters>& params) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!raw_device_id.empty());
  DCHECK(!params || params->IsValid());

  trace_scope->FinishedGettingAudioParameters();

  std::move(cb).Run(
      media::OUTPUT_DEVICE_STATUS_OK,
      params.value_or(media::AudioParameters::UnavailableDeviceParams()),
      raw_device_id, id_for_renderer);
}

}  // namespace content
