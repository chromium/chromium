// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "url/origin.h"

namespace content {

namespace {

AudioStreamBroker::LoopbackSource* GetLoopbackSourceOnUIThread(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* source = ForwardingAudioStreamFactory::CoreForFrame(
      (RenderFrameHost::FromID(render_process_id, render_frame_id)));
  if (!source) {
    // The source of the capture has already been destroyed, so fail early.
    return nullptr;
  }
  // Note: this pointer is sent over to the IO thread. This is safe since the
  // destruction of |source| is posted to the IO thread and it hasn't been
  // posted yet.
  return source;
}

void EnumerateOutputDevices(MediaStreamManager* media_stream_manager,
                            MediaDevicesManager::EnumerationCallback cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes device_types;
  device_types[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
  media_stream_manager->media_devices_manager()->EnumerateDevices(
      device_types, std::move(cb));
}

void TranslateDeviceId(const std::string& device_id,
                       const MediaDeviceSaltAndOrigin& salt_and_origin,
                       base::RepeatingCallback<void(const std::string&)> cb,
                       const MediaDeviceEnumeration& device_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& device_info :
       device_array[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) {
    if (MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt_and_origin.device_id_salt, salt_and_origin.origin, device_id,
            device_info.device_id)) {
      cb.Run(device_info.device_id);
      break;
    }
  }
  // If we're unable to translate the device id, |cb| will not be run.
}

void GetSaltOriginAndPermissionsOnUIThread(
    int process_id,
    int frame_id,
    base::OnceCallback<void(MediaDeviceSaltAndOrigin salt_and_origin,
                            bool has_access)> cb) {
  auto salt_and_origin = GetMediaDeviceSaltAndOrigin(process_id, frame_id);
  bool access = MediaDevicesPermissionChecker().CheckPermissionOnUIThread(
      blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, process_id, frame_id);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(std::move(cb), std::move(salt_and_origin), access));
}

}  // namespace

class RenderFrameAudioInputStreamFactory::Core final
    : public mojom::RendererAudioInputStreamFactory {
 public:
  Core(mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver,
       MediaStreamManager* media_stream_manager,
       RenderFrameHost* render_frame_host);

  ~Core() final;

  void Init(
      mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver);

  // mojom::RendererAudioInputStreamFactory implementation.
  void CreateStream(
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client,
      const base::UnguessableToken& session_id,
      const media::AudioParameters& audio_params,
      bool automatic_gain_control,
      uint32_t shared_memory_count,
      audio::mojom::AudioProcessingConfigPtr processing_config) final;

  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) final;

  void CreateLoopbackStream(
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client,
      const media::AudioParameters& audio_params,
      uint32_t shared_memory_count,
      bool disable_local_echo,
      AudioStreamBroker::LoopbackSource* loopback_source);

  void AssociateInputAndOutputForAecAfterCheckingAccess(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id,
      MediaDeviceSaltAndOrigin salt_and_origin,
      bool access_granted);

  void AssociateTranslatedOutputDeviceForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& raw_output_device_id);

  MediaStreamManager* const media_stream_manager_;
  const int process_id_;
  const int frame_id_;
  const url::Origin origin_;

  mojo::Receiver<RendererAudioInputStreamFactory> receiver_{this};
  // Always null-check this weak pointer before dereferencing it.
  base::WeakPtr<ForwardingAudioStreamFactory::Core> forwarding_factory_;

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

RenderFrameAudioInputStreamFactory::RenderFrameAudioInputStreamFactory(
    mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver,
    MediaStreamManager* media_stream_manager,
    RenderFrameHost* render_frame_host)
    : core_(new Core(std::move(receiver),
                     media_stream_manager,
                     render_frame_host)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RenderFrameAudioInputStreamFactory::~RenderFrameAudioInputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce([](std::unique_ptr<Core>) {}, std::move(core_)));
}

RenderFrameAudioInputStreamFactory::Core::Core(
    mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver,
    MediaStreamManager* media_stream_manager,
    RenderFrameHost* render_frame_host)
    : media_stream_manager_(media_stream_manager),
      process_id_(render_frame_host->GetProcess()->GetID()),
      frame_id_(render_frame_host->GetRoutingID()),
      origin_(render_frame_host->GetLastCommittedOrigin()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ForwardingAudioStreamFactory::Core* tmp_factory =
      ForwardingAudioStreamFactory::CoreForFrame(render_frame_host);

  if (!tmp_factory) {
    // The only case when we not have a forwarding factory at this point is when
    // the frame belongs to an interstitial. Interstitials don't need audio, so
    // it's fine to drop the receiver.
    return;
  }

  forwarding_factory_ = tmp_factory->AsWeakPtr();

  // Unretained is safe since the destruction of |this| is posted to the IO
  // thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Core::Init, base::Unretained(this), std::move(receiver)));
}

RenderFrameAudioInputStreamFactory::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void RenderFrameAudioInputStreamFactory::Core::Init(
    mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  receiver_.Bind(std::move(receiver));
}

void RenderFrameAudioInputStreamFactory::Core::CreateStream(
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client,
    const base::UnguessableToken& session_id,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    audio::mojom::AudioProcessingConfigPtr processing_config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT1("audio", "RenderFrameAudioInputStreamFactory::CreateStream",
               "session id", session_id.ToString());

  if (!forwarding_factory_)
    return;

  const blink::MediaStreamDevice* device =
      media_stream_manager_->audio_input_device_manager()->GetOpenedDeviceById(
          session_id);

  if (!device) {
    TRACE_EVENT_INSTANT0("audio", "device not found", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  WebContentsMediaCaptureId capture_id;
  if (WebContentsMediaCaptureId::Parse(device->id, &capture_id)) {
    // For MEDIA_GUM_DESKTOP_AUDIO_CAPTURE, the source is selected from
    // picker window, we do not mute the source audio. For
    // MEDIA_GUM_TAB_AUDIO_CAPTURE, the probable use case is Cast, we mute
    // the source audio.
    // TODO(qiangchen): Analyze audio constraints to make a duplicating or
    // diverting decision. It would give web developer more flexibility.

    base::PostTaskAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&GetLoopbackSourceOnUIThread,
                       capture_id.render_process_id,
                       capture_id.main_render_frame_id),
        base::BindOnce(
            &RenderFrameAudioInputStreamFactory::Core::CreateLoopbackStream,
            weak_ptr_factory_.GetWeakPtr(), std::move(client), audio_params,
            shared_memory_count, capture_id.disable_local_echo));

    if (device->type ==
        blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE)
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
    return;
  } else {
    forwarding_factory_->CreateInputStream(
        process_id_, frame_id_, device->id, audio_params, shared_memory_count,
        automatic_gain_control, std::move(processing_config),
        std::move(client));

    // Only count for captures from desktop media picker dialog and system loop
    // back audio.
    if (device->type ==
            blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE &&
        (media::AudioDeviceDescription::IsLoopbackDevice(device->id))) {
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
    }
  }
}

void RenderFrameAudioInputStreamFactory::Core::CreateLoopbackStream(
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client,
    const media::AudioParameters& audio_params,
    uint32_t shared_memory_count,
    bool disable_local_echo,
    AudioStreamBroker::LoopbackSource* loopback_source) {
  if (!loopback_source || !forwarding_factory_)
    return;

  forwarding_factory_->CreateLoopbackStream(
      process_id_, frame_id_, loopback_source, audio_params,
      shared_memory_count, disable_local_echo, std::move(client));
}

void RenderFrameAudioInputStreamFactory::Core::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!IsValidDeviceId(output_device_id))
    return;

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &GetSaltOriginAndPermissionsOnUIThread, process_id_, frame_id_,
          base::BindOnce(
              &Core::AssociateInputAndOutputForAecAfterCheckingAccess,
              weak_ptr_factory_.GetWeakPtr(), input_stream_id,
              output_device_id)));
}

void RenderFrameAudioInputStreamFactory::Core::
    AssociateInputAndOutputForAecAfterCheckingAccess(
        const base::UnguessableToken& input_stream_id,
        const std::string& output_device_id,
        MediaDeviceSaltAndOrigin salt_and_origin,
        bool access_granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!forwarding_factory_ || !access_granted)
    return;

  if (media::AudioDeviceDescription::IsDefaultDevice(output_device_id) ||
      media::AudioDeviceDescription::IsCommunicationsDevice(output_device_id)) {
    forwarding_factory_->AssociateInputAndOutputForAec(input_stream_id,
                                                       output_device_id);
  } else {
    EnumerateOutputDevices(
        media_stream_manager_,
        base::BindRepeating(
            &TranslateDeviceId, output_device_id, salt_and_origin,
            base::BindRepeating(&RenderFrameAudioInputStreamFactory::Core::
                                    AssociateTranslatedOutputDeviceForAec,
                                weak_ptr_factory_.GetWeakPtr(),
                                input_stream_id)));
  }
}

void RenderFrameAudioInputStreamFactory::Core::
    AssociateTranslatedOutputDeviceForAec(
        const base::UnguessableToken& input_stream_id,
        const std::string& raw_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!forwarding_factory_)
    return;
  forwarding_factory_->AssociateInputAndOutputForAec(input_stream_id,
                                                     raw_output_device_id);
}

}  // namespace content
