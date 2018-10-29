// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/old_render_frame_audio_input_stream_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_features.h"
#include "media/base/audio_parameters.h"

namespace content {
namespace {
void CheckPermissionAndGetSaltAndOrigin(
    const std::string& output_device_id,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(const MediaDeviceSaltAndOrigin&)> cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto salt_and_origin =
      GetMediaDeviceSaltAndOrigin(render_process_id, render_frame_id);

  // Check permissions for everything but the default device
  if (!media::AudioDeviceDescription::IsDefaultDevice(output_device_id) &&
      !MediaDevicesPermissionChecker().CheckPermissionOnUIThread(
          MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, render_process_id, render_frame_id)) {
    // If we're not allowed to use the device, don't call |cb|.
    return;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(std::move(cb), salt_and_origin));
}

void OldEnumerateOutputDevices(
    MediaDevicesManager* media_devices_manager,
    base::RepeatingCallback<
        void(const MediaDeviceSaltAndOrigin& salt_and_origin,
             const MediaDeviceEnumeration& devices)> cb,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes device_types;
  device_types[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  media_devices_manager->EnumerateDevices(device_types,
                                          base::BindOnce(cb, salt_and_origin));
}

}  // namespace

// static
std::unique_ptr<RenderFrameAudioInputStreamFactoryHandle,
                BrowserThread::DeleteOnIOThread>
RenderFrameAudioInputStreamFactoryHandle::CreateFactory(
    OldRenderFrameAudioInputStreamFactory::CreateDelegateCallback
        create_delegate_callback,
    content::MediaStreamManager* media_stream_manager,
    int render_process_id,
    int render_frame_id,
    mojom::RendererAudioInputStreamFactoryRequest request) {
  std::unique_ptr<RenderFrameAudioInputStreamFactoryHandle,
                  BrowserThread::DeleteOnIOThread>
      handle(new RenderFrameAudioInputStreamFactoryHandle(
          std::move(create_delegate_callback), media_stream_manager,
          render_process_id, render_frame_id));
  // Unretained is safe since |*handle| must be posted to the IO thread prior to
  // deletion.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&RenderFrameAudioInputStreamFactoryHandle::Init,
                     base::Unretained(handle.get()), std::move(request)));
  return handle;
}

RenderFrameAudioInputStreamFactoryHandle::
    ~RenderFrameAudioInputStreamFactoryHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

RenderFrameAudioInputStreamFactoryHandle::
    RenderFrameAudioInputStreamFactoryHandle(
        OldRenderFrameAudioInputStreamFactory::CreateDelegateCallback
            create_delegate_callback,
        MediaStreamManager* media_stream_manager,
        int render_process_id,
        int render_frame_id)
    : impl_(std::move(create_delegate_callback),
            media_stream_manager,
            render_process_id,
            render_frame_id),
      binding_(&impl_) {}

void RenderFrameAudioInputStreamFactoryHandle::Init(
    mojom::RendererAudioInputStreamFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  binding_.Bind(std::move(request));
}

OldRenderFrameAudioInputStreamFactory::OldRenderFrameAudioInputStreamFactory(
    CreateDelegateCallback create_delegate_callback,
    MediaStreamManager* media_stream_manager,
    int render_process_id,
    int render_frame_id)
    : create_delegate_callback_(std::move(create_delegate_callback)),
      media_stream_manager_(media_stream_manager),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_ptr_factory_(this) {
  DCHECK(create_delegate_callback_);
  // No thread-hostile state has been initialized yet, so we don't have to bind
  // to this specific thread.
}

OldRenderFrameAudioInputStreamFactory::
    ~OldRenderFrameAudioInputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void OldRenderFrameAudioInputStreamFactory::CreateStream(
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    int32_t session_id,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    audio::mojom::AudioProcessingConfigPtr processing_config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
// |processing_config| gets dropped here. It's not supported outside of the
// audio service. As this class is slated for removal, it will not be updated
// to support audio processing.
#if defined(OS_CHROMEOS)
  if (audio_params.channel_layout() ==
      media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    media_stream_manager_->audio_input_device_manager()
        ->RegisterKeyboardMicStream(base::BindOnce(
            &OldRenderFrameAudioInputStreamFactory::DoCreateStream,
            weak_ptr_factory_.GetWeakPtr(), std::move(client), session_id,
            audio_params, automatic_gain_control, shared_memory_count));
    return;
  }
#endif
  DoCreateStream(std::move(client), session_id, audio_params,
                 automatic_gain_control, shared_memory_count,
                 AudioInputDeviceManager::KeyboardMicRegistration());
}

void OldRenderFrameAudioInputStreamFactory::DoCreateStream(
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    int session_id,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    AudioInputDeviceManager::KeyboardMicRegistration
        keyboard_mic_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int stream_id = ++next_stream_id_;

  media::mojom::AudioLogPtr audio_log_ptr =
      MediaInternals::GetInstance()->CreateMojoAudioLog(
          media::AudioLogFactory::AUDIO_INPUT_CONTROLLER, stream_id,
          render_process_id_, render_frame_id_);

  // Unretained is safe since |this| owns |streams_|.
  streams_.insert(std::make_unique<AudioInputStreamHandle>(
      std::move(client),
      base::BindOnce(
          create_delegate_callback_,
          base::Unretained(media_stream_manager_->audio_input_device_manager()),
          std::move(audio_log_ptr), std::move(keyboard_mic_registration),
          shared_memory_count, stream_id, session_id, automatic_gain_control,
          audio_params),
      base::BindOnce(&OldRenderFrameAudioInputStreamFactory::RemoveStream,
                     weak_ptr_factory_.GetWeakPtr())));
}

void OldRenderFrameAudioInputStreamFactory::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!IsValidDeviceId(output_device_id))
    return;

  if (media::AudioDeviceDescription::IsDefaultDevice(output_device_id)) {
    for (const auto& stream : streams_) {
      if (stream->id() == input_stream_id) {
        stream->SetOutputDeviceForAec(output_device_id);
        return;
      }
    }
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            CheckPermissionAndGetSaltAndOrigin, output_device_id,
            render_process_id_, render_frame_id_,
            base::BindOnce(
                &OldEnumerateOutputDevices,
                media_stream_manager_->media_devices_manager(),
                base::BindRepeating(&OldRenderFrameAudioInputStreamFactory::
                                        TranslateAndSetOutputDeviceForAec,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    input_stream_id, output_device_id))));
  }
}

void OldRenderFrameAudioInputStreamFactory::TranslateAndSetOutputDeviceForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const MediaDeviceEnumeration& device_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string raw_output_device_id;
  for (const auto& device_info : device_array[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) {
    if (MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt_and_origin.device_id_salt, salt_and_origin.origin,
            output_device_id, device_info.device_id)) {
      raw_output_device_id = device_info.device_id;
    }
  }
  if (!raw_output_device_id.empty()) {
    for (const auto& stream : streams_) {
      if (stream->id() == input_stream_id) {
        stream->SetOutputDeviceForAec(raw_output_device_id);
        return;
      }
    }
  }
}

void OldRenderFrameAudioInputStreamFactory::RemoveStream(
    AudioInputStreamHandle* stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  streams_.erase(stream);
}

}  // namespace content
