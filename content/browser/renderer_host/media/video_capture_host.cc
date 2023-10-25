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
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"

namespace content {

namespace {

BrowserContext* GetBrowserContext(uint32_t render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (host) {
    return host->GetBrowserContext();
  }
  return nullptr;
}

}  // namespace

VideoCaptureHost::RenderProcessHostDelegate::~RenderProcessHostDelegate() =
    default;

// Looks up a RenderProcessHost on demand based on a given |render_process_id|
// and invokes OnMediaStreamAdded() and OnMediaStreamRemoved(). It should be
// called and destroyed on UI thread.
class VideoCaptureHost::RenderProcessHostDelegateImpl
    : public VideoCaptureHost::RenderProcessHostDelegate {
 public:
  explicit RenderProcessHostDelegateImpl(uint32_t render_process_id)
      : render_process_id_(render_process_id) {}

  RenderProcessHostDelegateImpl(const RenderProcessHostDelegateImpl&) = delete;
  RenderProcessHostDelegateImpl& operator=(
      const RenderProcessHostDelegateImpl&) = delete;

  ~RenderProcessHostDelegateImpl() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  // Helper functions that are used for notifying Browser-side RenderProcessHost
  // if renderer is currently consuming video capture. This information is then
  // used to determine if the renderer process should be backgrounded or not.
  void NotifyStreamAdded() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](uint32_t render_process_id) {
                         RenderProcessHost* host =
                             RenderProcessHost::FromID(render_process_id);
                         if (host) {
                           host->OnMediaStreamAdded();
                         }
                       },
                       render_process_id_));
  }

  void NotifyStreamRemoved() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](uint32_t render_process_id) {
                         RenderProcessHost* host =
                             RenderProcessHost::FromID(render_process_id);
                         if (host) {
                           host->OnMediaStreamRemoved();
                         }
                       },
                       render_process_id_));
  }

  uint32_t GetRenderProcessId() const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return render_process_id_;
  }

 private:
  const uint32_t render_process_id_;
};

VideoCaptureHost::VideoCaptureHost(uint32_t render_process_id,
                                   MediaStreamManager* media_stream_manager)
    : VideoCaptureHost(
          std::make_unique<RenderProcessHostDelegateImpl>(render_process_id),
          media_stream_manager) {}

VideoCaptureHost::VideoCaptureHost(
    std::unique_ptr<RenderProcessHostDelegate> delegate,
    MediaStreamManager* media_stream_manager)
    : render_process_host_delegate_(std::move(delegate)),
      media_stream_manager_(media_stream_manager) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void VideoCaptureHost::Create(
    uint32_t render_process_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_stream_manager->RegisterVideoCaptureHost(
      std::make_unique<VideoCaptureHost>(render_process_id,
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
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::Contains(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnNewBuffer(
        buffer_id, std::move(buffer_handle));
  }
}

void VideoCaptureHost::OnBufferDestroyed(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::Contains(device_id_to_observer_map_, controller_id))
    device_id_to_observer_map_[controller_id]->OnBufferDestroyed(buffer_id);
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& controller_id,
    const ReadyBuffer& buffer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (!base::Contains(device_id_to_observer_map_, controller_id))
    return;

  if (region_capture_rect_ != buffer.frame_info->metadata.region_capture_rect) {
    region_capture_rect_ = buffer.frame_info->metadata.region_capture_rect;
    media_stream_manager_->OnRegionCaptureRectChanged(controller_id,
                                                      region_capture_rect_);
  }

  media::mojom::ReadyBufferPtr mojom_buffer = media::mojom::ReadyBuffer::New(
      buffer.buffer_id, buffer.frame_info->Clone());
  device_id_to_observer_map_[controller_id]->OnBufferReady(
      std::move(mojom_buffer));
}

void VideoCaptureHost::OnFrameDropped(
    const VideoCaptureControllerID& controller_id,
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end() ||
      !base::Contains(device_id_to_observer_map_, controller_id)) {
    return;
  }
  device_id_to_observer_map_[controller_id]->OnFrameDropped(reason);
}

void VideoCaptureHost::OnFrameWithEmptyRegionCapture(
    const VideoCaptureControllerID& controller_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (region_capture_rect_ != absl::nullopt) {
    region_capture_rect_ = absl::nullopt;
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
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::Contains(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
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
  device_id_to_observer_map_[device_id].Bind(std::move(observer));

  const VideoCaptureControllerID controller_id(device_id);
  if (controllers_.find(controller_id) != controllers_.end()) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
            media::mojom::VideoCaptureState::STARTED));
    NotifyStreamAdded();
    return;
  }

  controllers_[controller_id] = base::WeakPtr<VideoCaptureController>();
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetBrowserContext,
                     render_process_host_delegate_->GetRenderProcessId()),
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

  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
            media::mojom::VideoCaptureState::STOPPED));
  }
  device_id_to_observer_map_.erase(controller_id);

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
  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
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
  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
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
  if (!base::Contains(controllers_, controller_id) ||
      !base::Contains(device_id_to_observer_map_, controller_id)) {
    return;
  }

  device_id_to_observer_map_[controller_id]->OnNewSubCaptureTargetVersion(
      sub_capture_target_version);
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

  if (base::Contains(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnStateChanged(
        media::mojom::VideoCaptureResult::NewErrorCode(error));
  }

  DeleteVideoCaptureController(controller_id, error);
  NotifyStreamRemoved();
}

void VideoCaptureHost::DoEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (base::Contains(device_id_to_observer_map_, controller_id)) {
    device_id_to_observer_map_[controller_id]->OnStateChanged(
        media::mojom::VideoCaptureResult::NewState(
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
    if (base::Contains(device_id_to_observer_map_, controller_id)) {
      device_id_to_observer_map_[device_id]->OnStateChanged(
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
  render_process_host_delegate_->NotifyStreamAdded();
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
  render_process_host_delegate_->NotifyStreamRemoved();
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
  media_stream_manager_->video_capture_manager()->ConnectClient(
      session_id, params, controller_id, this, std::move(done_cb),
      browser_context);
}

}  // namespace content
