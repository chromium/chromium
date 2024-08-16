// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

BrowserContext* GetBrowserContext(
    GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* host = RenderFrameHost::FromID(render_frame_host_id);
  if (host) {
    return host->GetBrowserContext();
  }
  return nullptr;
}

}  // namespace

VideoCaptureHost::RenderFrameHostDelegate::~RenderFrameHostDelegate() = default;

// Looks up a RenderFrameHost on demand based on a given |render_frame_host_id|
// and invokes OnMediaStreamAdded() and OnMediaStreamRemoved().
class VideoCaptureHost::RenderFrameHostDelegateImpl
    : public VideoCaptureHost::RenderFrameHostDelegate {
 public:
  explicit RenderFrameHostDelegateImpl(
      GlobalRenderFrameHostId render_frame_host_id)
      : render_frame_host_id_(render_frame_host_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  RenderFrameHostDelegateImpl(const RenderFrameHostDelegateImpl&) = delete;
  RenderFrameHostDelegateImpl& operator=(const RenderFrameHostDelegateImpl&) =
      delete;

  ~RenderFrameHostDelegateImpl() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  // Helper functions that are used for notifying Browser-side RenderFrameHost
  // if it is currently consuming video capture. This information is then used
  // to determine if the frame's renderer process should be backgrounded or not.
  void NotifyStreamAdded() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](GlobalRenderFrameHostId render_frame_host_id) {
                         RenderFrameHostImpl* host =
                             RenderFrameHostImpl::FromID(render_frame_host_id);
                         if (host) {
                           host->OnMediaStreamAdded(
                               RenderFrameHostImpl::MediaStreamType::
                                   kCapturingMediaStream);
                         }
                       },
                       render_frame_host_id_));
  }

  void NotifyStreamRemoved() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](GlobalRenderFrameHostId render_frame_host_id) {
                         RenderFrameHostImpl* host =
                             RenderFrameHostImpl::FromID(render_frame_host_id);
                         if (host && host->HasMediaStreams(
                                         RenderFrameHostImpl::MediaStreamType::
                                             kCapturingMediaStream)) {
                           host->OnMediaStreamRemoved(
                               RenderFrameHostImpl::MediaStreamType::
                                   kCapturingMediaStream);
                         }
                       },
                       render_frame_host_id_));
  }

  GlobalRenderFrameHostId GetRenderFrameHostId() const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return render_frame_host_id_;
  }

 private:
  const GlobalRenderFrameHostId render_frame_host_id_;
};

VideoCaptureHost::VideoCaptureHost(GlobalRenderFrameHostId render_frame_host_id,
                                   MediaStreamManager* media_stream_manager)
    : VideoCaptureHost(
          std::make_unique<RenderFrameHostDelegateImpl>(render_frame_host_id),
          media_stream_manager) {}

VideoCaptureHost::VideoCaptureHost(
    std::unique_ptr<RenderFrameHostDelegate> delegate,
    MediaStreamManager* media_stream_manager)
    : render_frame_host_delegate_(std::move(delegate)),
      media_stream_manager_(media_stream_manager) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void VideoCaptureHost::Create(
    GlobalRenderFrameHostId render_frame_host_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager->RegisterVideoCaptureHost(
      std::make_unique<VideoCaptureHost>(render_frame_host_id,
                                         media_stream_manager),
      std::move(receiver));
}

VideoCaptureHost::~VideoCaptureHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto it = controllers_.begin(); it != controllers_.end();) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller) {
      const VideoCaptureControllerID controller_id(it->first);
      media_stream_manager_->video_capture_manager()->DisconnectClient(
          controller.get(), controller_id, this,
          media::VideoCaptureError::kNone);
      ++it;
    } else {
      // Remove the entry for this controller_id so that when the controller
      // is added, the controller will be notified to stop for this client
      // in DoControllerAdded.
      controllers_.erase(it++);
    }
  }

  NotifyAllStreamsRemoved();
}

void VideoCaptureHost::OnError(const VideoCaptureControllerID& controller_id,
                               media::VideoCaptureError error) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureHost::DoError, weak_factory_.GetWeakPtr(),
                     controller_id, error));
}

void VideoCaptureHost::OnCaptureConfigurationChanged(
    const VideoCaptureControllerID& controller_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::Contains(controllers_, controller_id) ||
      !base::Contains(device_id_to_observer_map_, controller_id)) {
    return;
  }

  media_stream_manager_->OnCaptureConfigurationChanged(controller_id);
}

void VideoCaptureHost::OnNewBuffer(
    const VideoCaptureControllerID& controller_id,
    media::mojom::VideoBufferHandlePtr buffer_handle,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnNewBuffer(buffer_id, std::move(buffer_handle));
  }
}

void VideoCaptureHost::OnBufferDestroyed(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnBufferDestroyed(buffer_id);
  }
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& controller_id,
    const ReadyBuffer& buffer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it == device_id_to_observer_map_.end()) {
    return;
  }

  if (region_capture_rect_ != buffer.frame_info->metadata.region_capture_rect) {
    region_capture_rect_ = buffer.frame_info->metadata.region_capture_rect;
    media_stream_manager_->OnRegionCaptureRectChanged(controller_id,
                                                      region_capture_rect_);
  }

  media::mojom::ReadyBufferPtr mojom_buffer = media::mojom::ReadyBuffer::New(
      buffer.buffer_id, buffer.frame_info->Clone());
  it->second->OnBufferReady(std::move(mojom_buffer));
}

void VideoCaptureHost::OnFrameDropped(
    const VideoCaptureControllerID& controller_id,
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it == device_id_to_observer_map_.end()) {
    return;
  }

  it->second->OnFrameDropped(reason);
}

void VideoCaptureHost::OnFrameWithEmptyRegionCapture(
    const VideoCaptureControllerID& controller_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  if (region_capture_rect_ != std::nullopt) {
    region_capture_rect_ = std::nullopt;
    media_stream_manager_->OnRegionCaptureRectChanged(controller_id,
                                                      region_capture_rect_);
  }
}

void VideoCaptureHost::OnEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureHost::DoEnded,
                                weak_factory_.GetWeakPtr(), controller_id));
}

void VideoCaptureHost::OnStarted(
    const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
        media::mojom::VideoCaptureState::STARTED));
    NotifyStreamAdded();
  }
}

void VideoCaptureHost::OnStartedUsingGpuDecode(
    const VideoCaptureControllerID& id) {}

void VideoCaptureHost::Start(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer) {
  DVLOG(1) << __func__ << " session_id=" << session_id
           << ", device_id=" << device_id << ", format="
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureHost::Start");

  if (!params.IsValid()) {
    mojo::ReportBadMessage("Invalid video capture params.");
    return;
  }

  DCHECK(!base::Contains(device_id_to_observer_map_, device_id));
  auto& observer_in_map = device_id_to_observer_map_[device_id];
  observer_in_map.Bind(std::move(observer));

  const VideoCaptureControllerID controller_id(device_id);
  if (controllers_.find(controller_id) != controllers_.end()) {
    observer_in_map->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
        media::mojom::VideoCaptureState::STARTED));
    NotifyStreamAdded();
    return;
  }

  controllers_[controller_id] = base::WeakPtr<VideoCaptureController>();
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetBrowserContext,
                     render_frame_host_delegate_->GetRenderFrameHostId()),
      base::BindOnce(&VideoCaptureHost::ConnectClient,
                     weak_factory_.GetWeakPtr(), session_id, params,
                     controller_id,
                     base::BindOnce(&VideoCaptureHost::OnControllerAdded,
                                    weak_factory_.GetWeakPtr(), device_id)));
}

void VideoCaptureHost::Stop(const base::UnguessableToken& device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureHost::Stop");

  const VideoCaptureControllerID& controller_id(device_id);

  auto it = device_id_to_observer_map_.find(device_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
        media::mojom::VideoCaptureState::STOPPED));
    device_id_to_observer_map_.erase(it);
  }

  DeleteVideoCaptureController(controller_id, media::VideoCaptureError::kNone);
  NotifyStreamRemoved();
}

void VideoCaptureHost::Pause(const base::UnguessableToken& device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureHost::Pause");

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->PauseCaptureForClient(
      it->second.get(), controller_id, this);

  auto observer_it = device_id_to_observer_map_.find(device_id);
  if (observer_it != device_id_to_observer_map_.end()) {
    observer_it->second->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
            media::mojom::VideoCaptureState::PAUSED));
  }
}

void VideoCaptureHost::Resume(const base::UnguessableToken& device_id,
                              const base::UnguessableToken& session_id,
                              const media::VideoCaptureParams& params) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureHost::Resume");

  if (!params.IsValid()) {
    mojo::ReportBadMessage("Invalid video capture params.");
    return;
  }

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->ResumeCaptureForClient(
      session_id, params, it->second.get(), controller_id, this);

  auto observer_it = device_id_to_observer_map_.find(device_id);
  if (observer_it != device_id_to_observer_map_.end()) {
    observer_it->second->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
            media::mojom::VideoCaptureState::RESUMED));
  }
}

void VideoCaptureHost::RequestRefreshFrame(
    const base::UnguessableToken& device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  if (VideoCaptureController* controller = it->second.get()) {
    media_stream_manager_->video_capture_manager()
        ->RequestRefreshFrameForClient(controller);
  }
}

void VideoCaptureHost::ReleaseBuffer(
    const base::UnguessableToken& device_id,
    int32_t buffer_id,
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController>& controller = it->second;
  if (controller) {
    controller->ReturnBuffer(controller_id, this, buffer_id, feedback);
  }
}

void VideoCaptureHost::GetDeviceSupportedFormats(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    GetDeviceSupportedFormatsCallback callback) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats supported_formats;
  if (!media_stream_manager_->video_capture_manager()
           ->GetDeviceSupportedFormats(session_id, &supported_formats)) {
    DLOG(WARNING) << "Could not retrieve device supported formats";
  }
  std::move(callback).Run(supported_formats);
}

void VideoCaptureHost::GetDeviceFormatsInUse(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    GetDeviceFormatsInUseCallback callback) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats_in_use;
  if (!media_stream_manager_->video_capture_manager()->GetDeviceFormatsInUse(
          session_id, &formats_in_use)) {
    DLOG(WARNING) << "Could not retrieve device format(s) in use";
  }
  std::move(callback).Run(formats_in_use);
}

void VideoCaptureHost::OnNewSubCaptureTargetVersion(
    const base::UnguessableToken& device_id,
    uint32_t sub_capture_target_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const VideoCaptureControllerID controller_id(device_id);
  if (!base::Contains(controllers_, controller_id)) {
    return;
  }

  auto it = device_id_to_observer_map_.find(device_id);
  if (it == device_id_to_observer_map_.end()) {
    return;
  }

  it->second->OnNewSubCaptureTargetVersion(sub_capture_target_version);
}

void VideoCaptureHost::OnLog(const base::UnguessableToken& device_id,
                             const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController>& controller = it->second;
  if (controller)
    controller->OnLog(message);
}

void VideoCaptureHost::DoError(const VideoCaptureControllerID& controller_id,
                               media::VideoCaptureError error) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnStateChanged(
        media::mojom::VideoCaptureResult::NewErrorCode(error));
  }

  DeleteVideoCaptureController(controller_id, error);
  NotifyStreamRemoved();
}

void VideoCaptureHost::DoEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end()) {
    return;
  }

  auto it = device_id_to_observer_map_.find(controller_id);
  if (it != device_id_to_observer_map_.end()) {
    it->second->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
        media::mojom::VideoCaptureState::ENDED));
  }

  DeleteVideoCaptureController(controller_id, media::VideoCaptureError::kNone);
  NotifyStreamRemoved();
}

void VideoCaptureHost::OnControllerAdded(
    const base::UnguessableToken& device_id,
    const base::WeakPtr<VideoCaptureController>& controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end()) {
    if (controller) {
      media_stream_manager_->video_capture_manager()->DisconnectClient(
          controller.get(), controller_id, this,
          media::VideoCaptureError::kNone);
    }
    return;
  }

  if (!controller) {
    auto observer_it = device_id_to_observer_map_.find(device_id);
    if (observer_it != device_id_to_observer_map_.end()) {
      observer_it->second->OnStateChanged(
          media::mojom::VideoCaptureResult::NewErrorCode(
              media::VideoCaptureError::kVideoCaptureControllerInvalid));
    }
    controllers_.erase(controller_id);
    return;
  }

  DCHECK(!it->second);
  it->second = controller;
}

void VideoCaptureHost::DeleteVideoCaptureController(
    const VideoCaptureControllerID& controller_id,
    media::VideoCaptureError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController> controller = it->second;
  controllers_.erase(it);
  if (!controller)
    return;

  media_stream_manager_->video_capture_manager()->DisconnectClient(
      controller.get(), controller_id, this, error);
}

void VideoCaptureHost::NotifyStreamAdded() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ++number_of_active_streams_;
  render_frame_host_delegate_->NotifyStreamAdded();
}

void VideoCaptureHost::NotifyStreamRemoved() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // DoError() from camera side failure can be followed by Stop() from JS
  // side, so we should check before going to negative.
  // TODO(emircan): Investigate all edge cases and add more browsertests.
  // https://crbug.com/754765
  if (number_of_active_streams_ == 0)
    return;
  --number_of_active_streams_;
  render_frame_host_delegate_->NotifyStreamRemoved();
}

void VideoCaptureHost::NotifyAllStreamsRemoved() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  while (number_of_active_streams_ > 0)
    NotifyStreamRemoved();
}

void VideoCaptureHost::ConnectClient(const base::UnguessableToken session_id,
                                     const media::VideoCaptureParams& params,
                                     VideoCaptureControllerID controller_id,
                                     VideoCaptureManager::DoneCB done_cb,
                                     BrowserContext* browser_context) {
  std::optional<url::Origin> origin =
      media_stream_manager_->GetOriginByVideoSessionId(session_id);
  media_stream_manager_->video_capture_manager()->ConnectClient(
      session_id, params, controller_id, this, std::move(origin),
      std::move(done_cb), browser_context);
}

}  // namespace content
