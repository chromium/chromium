// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void BindMediaStreamDeviceObserverReceiver(
    int render_process_id,
    int render_frame_id,
    mojo::PendingReceiver<blink::mojom::MediaStreamDeviceObserver> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (render_frame_host)
    render_frame_host->GetRemoteInterfaces()->GetInterface(std::move(receiver));
}

}  // namespace

int MediaStreamDispatcherHost::next_requester_id_ = 0;

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      requester_id_(next_requester_id_++),
      media_stream_manager_(media_stream_manager),
      salt_and_origin_callback_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CancelAllRequests();
}

void MediaStreamDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaStreamDispatcherHost>(
          render_process_id, render_frame_id, media_stream_manager),
      std::move(receiver));
}

void MediaStreamDispatcherHost::OnDeviceStopped(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceStopped(label, device);
}

void MediaStreamDispatcherHost::OnDeviceChanged(
    const std::string& label,
    const blink::MediaStreamDevice& old_device,
    const blink::MediaStreamDevice& new_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceChanged(label, old_device,
                                                  new_device);
}

const mojo::Remote<blink::mojom::MediaStreamDeviceObserver>&
MediaStreamDispatcherHost::GetMediaStreamDeviceObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (media_stream_device_observer_)
    return media_stream_device_observer_;

  auto dispatcher_receiver =
      media_stream_device_observer_.BindNewPipeAndPassReceiver();
  media_stream_device_observer_.set_disconnect_handler(base::BindOnce(
      &MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError,
      weak_factory_.GetWeakPtr()));
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BindMediaStreamDeviceObserverReceiver, render_process_id_,
                     render_frame_id_, std::move(dispatcher_receiver)));
  return media_stream_device_observer_;
}

void MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_device_observer_.reset();
}

void MediaStreamDispatcherHost::CancelAllRequests() {
  media_stream_manager_->CancelAllRequests(render_process_id_, render_frame_id_,
                                           requester_id_);
}

void MediaStreamDispatcherHost::GenerateStream(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (audio_stream_selection_info_ptr->strategy ==
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_SESSION_ID &&
      (!audio_stream_selection_info_ptr->session_id.has_value() ||
       audio_stream_selection_info_ptr->session_id->is_empty())) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_STREAM_SELECTION_INFO);
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id_),
      base::BindOnce(&MediaStreamDispatcherHost::DoGenerateStream,
                     weak_factory_.GetWeakPtr(), page_request_id, controls,
                     user_gesture, std::move(audio_stream_selection_info_ptr),
                     std::move(callback)));
}

void MediaStreamDispatcherHost::DoGenerateStream(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.origin)) {
    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        std::string(), blink::MediaStreamDevices(),
        blink::MediaStreamDevices());
    return;
  }

  media_stream_manager_->GenerateStream(
      render_process_id_, render_frame_id_, requester_id_, page_request_id,
      controls, std::move(salt_and_origin), user_gesture,
      std::move(audio_stream_selection_info_ptr), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CancelRequest(int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id_,
                                       requester_id_, page_request_id);
}

void MediaStreamDispatcherHost::StopStreamDevice(
    const std::string& device_id,
    const base::Optional<base::UnguessableToken>& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->StopStreamDevice(
      render_process_id_, render_frame_id_, requester_id_, device_id,
      session_id.value_or(base::UnguessableToken()));
}

void MediaStreamDispatcherHost::OpenDevice(int32_t page_request_id,
                                           const std::string& device_id,
                                           blink::mojom::MediaStreamType type,
                                           OpenDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunner({BrowserThread::UI}).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id_),
      base::BindOnce(&MediaStreamDispatcherHost::DoOpenDevice,
                     weak_factory_.GetWeakPtr(), page_request_id, device_id,
                     type, std::move(callback)));
}

void MediaStreamDispatcherHost::DoOpenDevice(
    int32_t page_request_id,
    const std::string& device_id,
    blink::mojom::MediaStreamType type,
    OpenDeviceCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.origin)) {
    std::move(callback).Run(false /* success */, std::string(),
                            blink::MediaStreamDevice());
    return;
  }

  media_stream_manager_->OpenDevice(
      render_process_id_, render_frame_id_, requester_id_, page_request_id,
      device_id, type, std::move(salt_and_origin), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CloseDevice(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::SetCapturingLinkSecured(
    const base::Optional<base::UnguessableToken>& session_id,
    blink::mojom::MediaStreamType type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(
      render_process_id_, session_id.value_or(base::UnguessableToken()), type,
      is_secure);
}

void MediaStreamDispatcherHost::OnStreamStarted(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->OnStreamStarted(label);
}

}  // namespace content
