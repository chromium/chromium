// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "url/origin.h"

namespace content {

namespace {

std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr>
ToVectorAudioInputDeviceCapabilitiesPtr(
    const std::vector<blink::mojom::AudioInputDeviceCapabilities>&
        capabilities_vector,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr> result;
  result.reserve(capabilities_vector.size());
  for (auto& capabilities : capabilities_vector) {
    blink::mojom::AudioInputDeviceCapabilitiesPtr capabilities_ptr =
        blink::mojom::AudioInputDeviceCapabilities::New();
    capabilities_ptr->device_id =
        GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                                salt_and_origin.origin, capabilities.device_id);
    capabilities_ptr->group_id =
        GetHMACForMediaDeviceID(salt_and_origin.group_id_salt,
                                salt_and_origin.origin, capabilities.group_id);
    capabilities_ptr->parameters = capabilities.parameters;
    result.push_back(std::move(capabilities_ptr));
  }
  return result;
}

}  // namespace

// static
void MediaDevicesDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::MediaDevicesDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaDevicesDispatcherHost>(
          render_process_id, render_frame_id, media_stream_manager),
      std::move(receiver));
}

MediaDevicesDispatcherHost::MediaDevicesDispatcherHost(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      media_stream_manager_(media_stream_manager),
      num_pending_audio_input_parameters_(0) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MediaDevicesDispatcherHost::~MediaDevicesDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // It may happen that media_devices_manager() is destroyed before MDDH on some
  // shutdown scenarios.
  if (!media_stream_manager_->media_devices_manager())
    return;

  for (auto subscription_id : subscription_ids_) {
    media_stream_manager_->media_devices_manager()
        ->UnsubscribeDeviceChangeNotifications(subscription_id);
  }
}

void MediaDevicesDispatcherHost::EnumerateDevices(
    bool request_audio_input,
    bool request_video_input,
    bool request_audio_output,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if ((!request_audio_input && !request_video_input && !request_audio_output) ||
      (request_video_input_capabilities && !request_video_input) ||
      (request_audio_input_capabilities && !request_audio_input)) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] =
      request_audio_input;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      request_video_input;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] =
      request_audio_output;

  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      render_process_id_, render_frame_id_, devices_to_enumerate,
      request_video_input_capabilities, request_audio_input_capabilities,
      std::move(client_callback));
}

void MediaDevicesDispatcherHost::GetVideoInputCapabilities(
    GetVideoInputCapabilitiesCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(media_stream_manager_->media_devices_manager()
                         ->salt_and_origin_callback(),
                     render_process_id_, render_frame_id_),
      base::BindOnce(&MediaDevicesDispatcherHost::GetDefaultVideoInputDeviceID,
                     weak_factory_.GetWeakPtr(), std::move(client_callback)));
}

void MediaDevicesDispatcherHost::GetAllVideoInputDeviceFormats(
    const std::string& device_id,
    GetAllVideoInputDeviceFormatsCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetVideoInputDeviceFormats(device_id, false /* try_in_use_first */,
                             std::move(client_callback));
}

void MediaDevicesDispatcherHost::GetAvailableVideoInputDeviceFormats(
    const std::string& device_id,
    GetAvailableVideoInputDeviceFormatsCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetVideoInputDeviceFormats(device_id, true /* try_in_use_first */,
                             std::move(client_callback));
}

void MediaDevicesDispatcherHost::GetAudioInputCapabilities(
    GetAudioInputCapabilitiesCallback client_callback) {
  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(media_stream_manager_->media_devices_manager()
                         ->salt_and_origin_callback(),
                     render_process_id_, render_frame_id_),
      base::BindOnce(&MediaDevicesDispatcherHost::GetDefaultAudioInputDeviceID,
                     weak_factory_.GetWeakPtr(), std::move(client_callback)));
}

void MediaDevicesDispatcherHost::AddMediaDevicesListener(
    bool subscribe_audio_input,
    bool subscribe_video_input,
    bool subscribe_audio_output,
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!subscribe_audio_input && !subscribe_video_input &&
      !subscribe_audio_output) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  MediaDevicesManager::BoolDeviceTypes devices_to_subscribe;
  devices_to_subscribe[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] =
      subscribe_audio_input;
  devices_to_subscribe[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      subscribe_video_input;
  devices_to_subscribe[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] =
      subscribe_audio_output;

  uint32_t subscription_id = media_stream_manager_->media_devices_manager()
                                 ->SubscribeDeviceChangeNotifications(
                                     render_process_id_, render_frame_id_,
                                     devices_to_subscribe, std::move(listener));
  subscription_ids_.push_back(subscription_id);
}

void MediaDevicesDispatcherHost::GetDefaultVideoInputDeviceID(
    GetVideoInputCapabilitiesCallback client_callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetDefaultMediaDeviceID(
      blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT, render_process_id_,
      render_frame_id_,
      base::Bind(&MediaDevicesDispatcherHost::GotDefaultVideoInputDeviceID,
                 weak_factory_.GetWeakPtr(), base::Passed(&client_callback),
                 std::move(salt_and_origin)));
}

void MediaDevicesDispatcherHost::GotDefaultVideoInputDeviceID(
    GetVideoInputCapabilitiesCallback client_callback,
    MediaDeviceSaltAndOrigin salt_and_origin,
    const std::string& default_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes requested_types;
  // Also request audio devices to make sure the heuristic to determine
  // the video group ID works.
  requested_types[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::BindOnce(
          &MediaDevicesDispatcherHost::FinalizeGetVideoInputCapabilities,
          weak_factory_.GetWeakPtr(), std::move(client_callback),
          std::move(salt_and_origin), std::move(default_device_id)));
}

void MediaDevicesDispatcherHost::FinalizeGetVideoInputCapabilities(
    GetVideoInputCapabilitiesCallback client_callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& default_device_id,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
      video_input_capabilities;
  for (const auto& device_info :
       enumeration[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT]) {
    std::string hmac_device_id =
        GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                                salt_and_origin.origin, device_info.device_id);
    std::string hmac_group_id =
        GetHMACForMediaDeviceID(salt_and_origin.group_id_salt,
                                salt_and_origin.origin, device_info.group_id);
    blink::mojom::VideoInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::VideoInputDeviceCapabilities::New();
    capabilities->device_id = std::move(hmac_device_id);
    capabilities->group_id = std::move(hmac_group_id);
    capabilities->formats =
        media_stream_manager_->media_devices_manager()->GetVideoInputFormats(
            device_info.device_id, true /* try_in_use_first */);
    capabilities->facing_mode = device_info.video_facing;
    if (device_info.device_id == default_device_id) {
      video_input_capabilities.insert(video_input_capabilities.begin(),
                                      std::move(capabilities));
    } else {
      video_input_capabilities.push_back(std::move(capabilities));
    }
  }

  std::move(client_callback).Run(std::move(video_input_capabilities));
}

void MediaDevicesDispatcherHost::GetVideoInputDeviceFormats(
    const std::string& device_id,
    bool try_in_use_first,
    GetVideoInputDeviceFormatsCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(media_stream_manager_->media_devices_manager()
                         ->salt_and_origin_callback(),
                     render_process_id_, render_frame_id_),
      base::BindOnce(
          &MediaDevicesDispatcherHost::EnumerateVideoDevicesForFormats,
          weak_factory_.GetWeakPtr(), std::move(client_callback), device_id,
          try_in_use_first));
}

void MediaDevicesDispatcherHost::EnumerateVideoDevicesForFormats(
    GetVideoInputDeviceFormatsCallback client_callback,
    const std::string& device_id,
    bool try_in_use_first,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager_->video_capture_manager()->EnumerateDevices(
      base::BindOnce(
          &MediaDevicesDispatcherHost::FinalizeGetVideoInputDeviceFormats,
          weak_factory_.GetWeakPtr(), std::move(client_callback), device_id,
          try_in_use_first, salt_and_origin.device_id_salt,
          salt_and_origin.origin));
}

void MediaDevicesDispatcherHost::FinalizeGetVideoInputDeviceFormats(
    GetVideoInputDeviceFormatsCallback client_callback,
    const std::string& device_id,
    bool try_in_use_first,
    const std::string& device_id_salt,
    const url::Origin& security_origin,
    const media::VideoCaptureDeviceDescriptors& device_descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& descriptor : device_descriptors) {
    if (DoesMediaDeviceIDMatchHMAC(device_id_salt, security_origin, device_id,
                                   descriptor.device_id)) {
      std::move(client_callback)
          .Run(media_stream_manager_->media_devices_manager()
                   ->GetVideoInputFormats(descriptor.device_id,
                                          try_in_use_first));
      return;
    }
  }
  std::move(client_callback).Run(media::VideoCaptureFormats());
}

struct MediaDevicesDispatcherHost::AudioInputCapabilitiesRequest {
  MediaDeviceSaltAndOrigin salt_and_origin;
  GetAudioInputCapabilitiesCallback client_callback;
};

void MediaDevicesDispatcherHost::GetDefaultAudioInputDeviceID(
    GetAudioInputCapabilitiesCallback client_callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  pending_audio_input_capabilities_requests_.push_back(
      AudioInputCapabilitiesRequest{salt_and_origin,
                                    std::move(client_callback)});
  if (pending_audio_input_capabilities_requests_.size() > 1U)
    return;

  DCHECK(current_audio_input_capabilities_.empty());
  GetDefaultMediaDeviceID(
      blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT, render_process_id_,
      render_frame_id_,
      base::Bind(&MediaDevicesDispatcherHost::GotDefaultAudioInputDeviceID,
                 weak_factory_.GetWeakPtr()));
}

void MediaDevicesDispatcherHost::GotDefaultAudioInputDeviceID(
    const std::string& default_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK(current_audio_input_capabilities_.empty());
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
  media_stream_manager_->media_devices_manager()->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaDevicesDispatcherHost::GotAudioInputEnumeration,
                     weak_factory_.GetWeakPtr(), default_device_id));
}

void MediaDevicesDispatcherHost::GotAudioInputEnumeration(
    const std::string& default_device_id,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK(current_audio_input_capabilities_.empty());
  DCHECK_EQ(num_pending_audio_input_parameters_, 0U);
  for (const auto& device_info :
       enumeration[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT]) {
    auto parameters = media::AudioParameters::UnavailableDeviceParams();
    blink::mojom::AudioInputDeviceCapabilities capabilities(
        device_info.device_id, device_info.group_id, parameters,
        parameters.IsValid(), parameters.channels(), parameters.sample_rate(),
        parameters.GetBufferDuration());
    if (device_info.device_id == default_device_id)
      current_audio_input_capabilities_.insert(
          current_audio_input_capabilities_.begin(), std::move(capabilities));
    else
      current_audio_input_capabilities_.push_back(std::move(capabilities));
  }
  // No devices or fake devices, no need to read audio parameters.
  if (current_audio_input_capabilities_.empty() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    FinalizeGetAudioInputCapabilities();
    return;
  }

  num_pending_audio_input_parameters_ =
      current_audio_input_capabilities_.size();
  for (size_t i = 0; i < num_pending_audio_input_parameters_; ++i) {
    media_stream_manager_->audio_system()->GetInputStreamParameters(
        current_audio_input_capabilities_[i].device_id,
        base::BindOnce(&MediaDevicesDispatcherHost::GotAudioInputParameters,
                       weak_factory_.GetWeakPtr(), i));
  }
}

void MediaDevicesDispatcherHost::GotAudioInputParameters(
    size_t index,
    const base::Optional<media::AudioParameters>& parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK_GT(current_audio_input_capabilities_.size(), index);
  DCHECK_GT(num_pending_audio_input_parameters_, 0U);

  if (parameters)
    current_audio_input_capabilities_[index].parameters = *parameters;
  DCHECK(current_audio_input_capabilities_[index].parameters.IsValid());
  if (--num_pending_audio_input_parameters_ == 0U)
    FinalizeGetAudioInputCapabilities();
}

void MediaDevicesDispatcherHost::FinalizeGetAudioInputCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(pending_audio_input_capabilities_requests_.size(), 0U);
  DCHECK_EQ(num_pending_audio_input_parameters_, 0U);

  for (auto& request : pending_audio_input_capabilities_requests_) {
    std::move(request.client_callback)
        .Run(ToVectorAudioInputDeviceCapabilitiesPtr(
            current_audio_input_capabilities_, request.salt_and_origin));
  }

  current_audio_input_capabilities_.clear();
  pending_audio_input_capabilities_requests_.clear();
}

}  // namespace content
