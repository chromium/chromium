// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_device_manager.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/stream/media_stream_device_observer.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "media/media_buildflags.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

const char kPepperInsecureOriginMessage[] =
    "Microphone and Camera access no longer works on insecure origins. To use "
    "this feature, you should consider switching your application to a "
    "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
    "details.";

PP_DeviceType_Dev FromMediaDeviceType(MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return PP_DEVICETYPE_DEV_AUDIOCAPTURE;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return PP_DEVICETYPE_DEV_VIDEOCAPTURE;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return PP_DEVICETYPE_DEV_AUDIOOUTPUT;
    default:
      NOTREACHED();
      return PP_DEVICETYPE_DEV_INVALID;
  }
}

MediaDeviceType ToMediaDeviceType(PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return MEDIA_DEVICE_TYPE_AUDIO_INPUT;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return MEDIA_DEVICE_TYPE_VIDEO_INPUT;
    case PP_DEVICETYPE_DEV_AUDIOOUTPUT:
      return MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
    default:
      NOTREACHED();
      return MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  }
}

ppapi::DeviceRefData FromMediaDeviceInfo(MediaDeviceType type,
                                         const MediaDeviceInfo& info) {
  ppapi::DeviceRefData data;
  data.id = info.device_id;
  // Some Flash content can't handle an empty string, so stick a space in to
  // make them happy. See crbug.com/408404.
  data.name = info.label.empty() ? std::string(" ") : info.label;
  data.type = FromMediaDeviceType(type);
  return data;
}

std::vector<ppapi::DeviceRefData> FromMediaDeviceInfoArray(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  std::vector<ppapi::DeviceRefData> devices;
  devices.reserve(device_infos.size());
  for (const auto& device_info : device_infos)
    devices.push_back(FromMediaDeviceInfo(type, device_info));

  return devices;
}

}  // namespace

base::WeakPtr<PepperMediaDeviceManager>
PepperMediaDeviceManager::GetForRenderFrame(
    RenderFrame* render_frame) {
  PepperMediaDeviceManager* handler =
      PepperMediaDeviceManager::Get(render_frame);
  if (!handler)
    handler = new PepperMediaDeviceManager(render_frame);
  return handler->AsWeakPtr();
}

PepperMediaDeviceManager::PepperMediaDeviceManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<PepperMediaDeviceManager>(render_frame) {}

PepperMediaDeviceManager::~PepperMediaDeviceManager() {
  DCHECK(open_callbacks_.empty());
}

void PepperMediaDeviceManager::EnumerateDevices(
    PP_DeviceType_Dev type,
    const DevicesCallback& callback) {
  bool request_audio_input = type == PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  bool request_video_input = type == PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  bool request_audio_output = type == PP_DEVICETYPE_DEV_AUDIOOUTPUT;
  CHECK(request_audio_input || request_video_input || request_audio_output);
  GetMediaDevicesDispatcher()->EnumerateDevices(
      request_audio_input, request_video_input, request_audio_output,
      false /* request_video_input_capabilities */,
      base::BindOnce(&PepperMediaDeviceManager::DevicesEnumerated, AsWeakPtr(),
                     callback, ToMediaDeviceType(type)));
}

size_t PepperMediaDeviceManager::StartMonitoringDevices(
    PP_DeviceType_Dev type,
    const DevicesCallback& callback) {
  bool subscribe_audio_input = type == PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  bool subscribe_video_input = type == PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  bool subscribe_audio_output = type == PP_DEVICETYPE_DEV_AUDIOOUTPUT;
  CHECK(subscribe_audio_input || subscribe_video_input ||
        subscribe_audio_output);
  blink::mojom::MediaDevicesListenerPtr listener;
  size_t subscription_id =
      bindings_.AddBinding(this, mojo::MakeRequest(&listener));
  GetMediaDevicesDispatcher()->AddMediaDevicesListener(
      subscribe_audio_input, subscribe_video_input, subscribe_audio_output,
      std::move(listener));
  SubscriptionList& subscriptions =
      device_change_subscriptions_[ToMediaDeviceType(type)];
  subscriptions.push_back(Subscription{subscription_id, callback});

  return subscription_id;
}

void PepperMediaDeviceManager::StopMonitoringDevices(PP_DeviceType_Dev type,
                                                     size_t subscription_id) {
  SubscriptionList& subscriptions =
      device_change_subscriptions_[ToMediaDeviceType(type)];
  base::EraseIf(subscriptions,
                [subscription_id](const Subscription& subscription) {
                  return subscription.first == subscription_id;
                });
  bindings_.RemoveBinding(subscription_id);
}

int PepperMediaDeviceManager::OpenDevice(PP_DeviceType_Dev type,
                                         const std::string& device_id,
                                         PP_Instance pp_instance,
                                         const OpenDeviceCallback& callback) {
  open_callbacks_[next_id_] = callback;
  int request_id = next_id_++;

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(pp_instance);
  if (!host->IsSecureContext(pp_instance)) {
    RenderFrame* render_frame = host->GetRenderFrameForInstance(pp_instance);
    if (render_frame) {
      render_frame->AddMessageToConsole(CONSOLE_MESSAGE_LEVEL_WARNING,
                                        kPepperInsecureOriginMessage);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&PepperMediaDeviceManager::OnDeviceOpened, AsWeakPtr(),
                       request_id, false, std::string(), MediaStreamDevice()));
    return request_id;
  }

  GetMediaStreamDispatcherHost()->OpenDevice(
      request_id, device_id,
      PepperMediaDeviceManager::FromPepperDeviceType(type),
      base::BindOnce(&PepperMediaDeviceManager::OnDeviceOpened, AsWeakPtr(),
                     request_id));

  return request_id;
}

void PepperMediaDeviceManager::CancelOpenDevice(int request_id) {
  open_callbacks_.erase(request_id);

  GetMediaStreamDispatcherHost()->CancelRequest(request_id);
}

void PepperMediaDeviceManager::CloseDevice(const std::string& label) {
  if (!GetMediaStreamDeviceObserver()->RemoveStream(label))
    return;

  GetMediaStreamDispatcherHost()->CloseDevice(label);
}

int PepperMediaDeviceManager::GetSessionID(PP_DeviceType_Dev type,
                                           const std::string& label) {
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return GetMediaStreamDeviceObserver()->audio_session_id(label);
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return GetMediaStreamDeviceObserver()->video_session_id(label);
    default:
      NOTREACHED();
      return 0;
  }
}

// static
MediaStreamType PepperMediaDeviceManager::FromPepperDeviceType(
    PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_INVALID:
      return MEDIA_NO_SERVICE;
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return MEDIA_DEVICE_AUDIO_CAPTURE;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return MEDIA_DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
      return MEDIA_NO_SERVICE;
  }
}

void PepperMediaDeviceManager::OnDevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  std::vector<ppapi::DeviceRefData> devices =
      FromMediaDeviceInfoArray(type, device_infos);
  SubscriptionList& subscriptions = device_change_subscriptions_[type];
  for (auto& subscription : subscriptions)
    subscription.second.Run(devices);
}

void PepperMediaDeviceManager::OnDeviceOpened(int request_id,
                                              bool success,
                                              const std::string& label,
                                              const MediaStreamDevice& device) {
  auto iter = open_callbacks_.find(request_id);
  if (iter == open_callbacks_.end()) {
    // The callback may have been unregistered.
    return;
  }

  if (success)
    GetMediaStreamDeviceObserver()->AddStream(label, device);

  OpenDeviceCallback callback = iter->second;
  open_callbacks_.erase(iter);

  std::move(callback).Run(request_id, success, success ? label : std::string());
}

void PepperMediaDeviceManager::DevicesEnumerated(
    const DevicesCallback& client_callback,
    MediaDeviceType type,
    const std::vector<MediaDeviceInfoArray>& enumeration,
    std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  client_callback.Run(FromMediaDeviceInfoArray(type, enumeration[type]));
}

const mojom::MediaStreamDispatcherHostPtr&
PepperMediaDeviceManager::GetMediaStreamDispatcherHost() {
  if (!dispatcher_host_) {
    CHECK(render_frame());
    CHECK(render_frame()->GetRemoteInterfaces());
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&dispatcher_host_));
  }
  return dispatcher_host_;
}

MediaStreamDeviceObserver*
PepperMediaDeviceManager::GetMediaStreamDeviceObserver() const {
  DCHECK(render_frame());
  MediaStreamDeviceObserver* const observer =
      static_cast<RenderFrameImpl*>(render_frame())
          ->GetMediaStreamDeviceObserver();
  DCHECK(observer);
  return observer;
}

const blink::mojom::MediaDevicesDispatcherHostPtr&
PepperMediaDeviceManager::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    CHECK(render_frame());
    CHECK(render_frame()->GetRemoteInterfaces());
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

void PepperMediaDeviceManager::OnDestruct() {
  delete this;
}

}  // namespace content
