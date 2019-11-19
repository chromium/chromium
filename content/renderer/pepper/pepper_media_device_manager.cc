// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_device_manager.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/content_features.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"

namespace content {

namespace {

const char kPepperInsecureOriginMessage[] =
    "Microphone and Camera access no longer works on insecure origins. To use "
    "this feature, you should consider switching your application to a "
    "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
    "details.";

PP_DeviceType_Dev FromMediaDeviceType(blink::MediaDeviceType type) {
  switch (type) {
    case blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return PP_DEVICETYPE_DEV_AUDIOCAPTURE;
    case blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return PP_DEVICETYPE_DEV_VIDEOCAPTURE;
    case blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return PP_DEVICETYPE_DEV_AUDIOOUTPUT;
    default:
      NOTREACHED();
      return PP_DEVICETYPE_DEV_INVALID;
  }
}

blink::MediaDeviceType ToMediaDeviceType(PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT;
    case PP_DEVICETYPE_DEV_AUDIOOUTPUT:
      return blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
    default:
      NOTREACHED();
      return blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  }
}

ppapi::DeviceRefData FromMediaDeviceInfo(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfo& info) {
  ppapi::DeviceRefData data;
  data.id = info.device_id;
  // Some Flash content can't handle an empty string, so stick a space in to
  // make them happy. See crbug.com/408404.
  data.name = info.label.empty() ? std::string(" ") : info.label;
  data.type = FromMediaDeviceType(type);
  return data;
}

std::vector<ppapi::DeviceRefData> FromMediaDeviceInfoArray(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos) {
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

void PepperMediaDeviceManager::EnumerateDevices(PP_DeviceType_Dev type,
                                                DevicesOnceCallback callback) {
  bool request_audio_input = type == PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  bool request_video_input = type == PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  bool request_audio_output = type == PP_DEVICETYPE_DEV_AUDIOOUTPUT;
  CHECK(request_audio_input || request_video_input || request_audio_output);
  GetMediaDevicesDispatcher()->EnumerateDevices(
      request_audio_input, request_video_input, request_audio_output,
      false /* request_video_input_capabilities */,
      false /* request_audio_input_capabilities */,
      base::BindOnce(&PepperMediaDeviceManager::DevicesEnumerated, AsWeakPtr(),
                     std::move(callback), ToMediaDeviceType(type)));
}

size_t PepperMediaDeviceManager::StartMonitoringDevices(
    PP_DeviceType_Dev type,
    const DevicesCallback& callback) {
  bool subscribe_audio_input = type == PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  bool subscribe_video_input = type == PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  bool subscribe_audio_output = type == PP_DEVICETYPE_DEV_AUDIOOUTPUT;
  CHECK(subscribe_audio_input || subscribe_video_input ||
        subscribe_audio_output);
  mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener;
  size_t subscription_id =
      receivers_.Add(this, listener.InitWithNewPipeAndPassReceiver());
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
  receivers_.Remove(subscription_id);
}

int PepperMediaDeviceManager::OpenDevice(PP_DeviceType_Dev type,
                                         const std::string& device_id,
                                         PP_Instance pp_instance,
                                         OpenDeviceCallback callback) {
  open_callbacks_[next_id_] = std::move(callback);
  int request_id = next_id_++;

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(pp_instance);
  if (!host->IsSecureContext(pp_instance)) {
    RenderFrame* render_frame = host->GetRenderFrameForInstance(pp_instance);
    if (render_frame) {
      render_frame->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          kPepperInsecureOriginMessage);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PepperMediaDeviceManager::OnDeviceOpened,
                                  AsWeakPtr(), request_id, false, std::string(),
                                  blink::MediaStreamDevice()));
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
  if (!GetMediaStreamDeviceObserver()->RemoveStream(
          blink::WebString::FromUTF8(label)))
    return;

  GetMediaStreamDispatcherHost()->CloseDevice(label);
}

base::UnguessableToken PepperMediaDeviceManager::GetSessionID(
    PP_DeviceType_Dev type,
    const std::string& label) {
  switch (type) {
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return GetMediaStreamDeviceObserver()->GetAudioSessionId(
          blink::WebString::FromUTF8(label));
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return GetMediaStreamDeviceObserver()->GetVideoSessionId(
          blink::WebString::FromUTF8(label));
    default:
      NOTREACHED();
      return base::UnguessableToken();
  }
}

// static
blink::mojom::MediaStreamType PepperMediaDeviceManager::FromPepperDeviceType(
    PP_DeviceType_Dev type) {
  switch (type) {
    case PP_DEVICETYPE_DEV_INVALID:
      return blink::mojom::MediaStreamType::NO_SERVICE;
    case PP_DEVICETYPE_DEV_AUDIOCAPTURE:
      return blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
    case PP_DEVICETYPE_DEV_VIDEOCAPTURE:
      return blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
      return blink::mojom::MediaStreamType::NO_SERVICE;
  }
}

void PepperMediaDeviceManager::OnDevicesChanged(
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  std::vector<ppapi::DeviceRefData> devices =
      FromMediaDeviceInfoArray(type, device_infos);
  SubscriptionList& subscriptions = device_change_subscriptions_[type];
  for (auto& subscription : subscriptions)
    subscription.second.Run(devices);
}

void PepperMediaDeviceManager::OnDeviceOpened(
    int request_id,
    bool success,
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  auto iter = open_callbacks_.find(request_id);
  if (iter == open_callbacks_.end()) {
    // The callback may have been unregistered.
    return;
  }

  if (success)
    GetMediaStreamDeviceObserver()->AddStream(blink::WebString::FromUTF8(label),
                                              device);

  OpenDeviceCallback callback = std::move(iter->second);
  open_callbacks_.erase(iter);

  std::move(callback).Run(request_id, success, success ? label : std::string());
}

void PepperMediaDeviceManager::DevicesEnumerated(
    DevicesOnceCallback client_callback,
    blink::MediaDeviceType type,
    const std::vector<blink::WebMediaDeviceInfoArray>& enumeration,
    std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  std::move(client_callback)
      .Run(FromMediaDeviceInfoArray(type, enumeration[type]));
}

blink::mojom::MediaStreamDispatcherHost*
PepperMediaDeviceManager::GetMediaStreamDispatcherHost() {
  if (!dispatcher_host_) {
    CHECK(render_frame());
    render_frame()->GetBrowserInterfaceBroker()->GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver());
  }
  return dispatcher_host_.get();
}

blink::WebMediaStreamDeviceObserver*
PepperMediaDeviceManager::GetMediaStreamDeviceObserver() const {
  DCHECK(render_frame());
  blink::WebMediaStreamDeviceObserver* const observer =
      static_cast<RenderFrameImpl*>(render_frame())
          ->MediaStreamDeviceObserver();
  DCHECK(observer);
  return observer;
}

blink::mojom::MediaDevicesDispatcherHost*
PepperMediaDeviceManager::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    CHECK(render_frame());
    render_frame()->GetBrowserInterfaceBroker()->GetInterface(
        media_devices_dispatcher_.BindNewPipeAndPassReceiver());
  }

  return media_devices_dispatcher_.get();
}

void PepperMediaDeviceManager::OnDestruct() {
  delete this;
}

}  // namespace content
