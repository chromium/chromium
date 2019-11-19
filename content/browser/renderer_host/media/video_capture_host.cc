// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

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

  ~RenderProcessHostDelegateImpl() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // Helper functions that are used for notifying Browser-side RenderProcessHost
  // if renderer is currently consuming video capture. This information is then
  // used to determine if the renderer process should be backgrounded or not.
  void NotifyStreamAdded() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
    if (host)
      host->OnMediaStreamAdded();
  }

  void NotifyStreamRemoved() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
    if (host)
      host->OnMediaStreamRemoved();
  }

 private:
  const uint32_t render_process_id_;
  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostDelegateImpl);
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
  mojo::MakeSelfOwnedReceiver(std::make_unique<VideoCaptureHost>(
                                  render_process_id, media_stream_manager),
                              std::move(receiver));
}

VideoCaptureHost::~VideoCaptureHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto it = controllers_.begin(); it != controllers_.end(); ) {
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
  base::DeleteSoon(FROM_HERE, {BrowserThread::UI},
                   render_process_host_delegate_.release());
}

void VideoCaptureHost::OnError(const VideoCaptureControllerID& controller_id,
                               media::VideoCaptureError error) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&VideoCaptureHost::DoError, weak_factory_.GetWeakPtr(),
                     controller_id, error));
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
    int buffer_id,
    const media::mojom::VideoFrameInfoPtr& frame_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (controllers_.find(controller_id) == controllers_.end())
    return;

  if (!base::Contains(device_id_to_observer_map_, controller_id))
    return;

  device_id_to_observer_map_[controller_id]->OnBufferReady(buffer_id,
                                                           frame_info.Clone());
}

void VideoCaptureHost::OnEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&VideoCaptureHost::DoEnded,
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
        media::mojom::VideoCaptureState::STARTED);
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

  DCHECK(!base::Contains(device_id_to_observer_map_, device_id));
  device_id_to_observer_map_[device_id].Bind(std::move(observer));

  const VideoCaptureControllerID controller_id(device_id);
  if (controllers_.find(controller_id) != controllers_.end()) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureState::STARTED);
    NotifyStreamAdded();
    return;
  }

  controllers_[controller_id] = base::WeakPtr<VideoCaptureController>();
  media_stream_manager_->video_capture_manager()->ConnectClient(
      session_id, params, controller_id, this,
      base::Bind(&VideoCaptureHost::OnControllerAdded,
                 weak_factory_.GetWeakPtr(), device_id));
}

void VideoCaptureHost::Stop(const base::UnguessableToken& device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const VideoCaptureControllerID& controller_id(device_id);

  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureState::STOPPED);
  }
  device_id_to_observer_map_.erase(controller_id);

  DeleteVideoCaptureController(controller_id, media::VideoCaptureError::kNone);
  NotifyStreamRemoved();
}

void VideoCaptureHost::Pause(const base::UnguessableToken& device_id) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->PauseCaptureForClient(
      it->second.get(), controller_id, this);
  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureState::PAUSED);
  }
}

void VideoCaptureHost::Resume(const base::UnguessableToken& device_id,
                              const base::UnguessableToken& session_id,
                              const media::VideoCaptureParams& params) {
  DVLOG(1) << __func__ << " " << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end() || !it->second)
    return;

  media_stream_manager_->video_capture_manager()->ResumeCaptureForClient(
      session_id, params, it->second.get(), controller_id, this);
  if (base::Contains(device_id_to_observer_map_, device_id)) {
    device_id_to_observer_map_[device_id]->OnStateChanged(
        media::mojom::VideoCaptureState::RESUMED);
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

void VideoCaptureHost::ReleaseBuffer(const base::UnguessableToken& device_id,
                                     int32_t buffer_id,
                                     double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController>& controller = it->second;
  if (controller) {
    controller->ReturnBuffer(controller_id, this, buffer_id,
                             consumer_resource_utilization);
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

void VideoCaptureHost::OnFrameDropped(
    const base::UnguessableToken& device_id,
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureControllerID controller_id(device_id);
  auto it = controllers_.find(controller_id);
  if (it == controllers_.end())
    return;

  const base::WeakPtr<VideoCaptureController>& controller = it->second;
  if (controller)
    controller->OnFrameDropped(reason);
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
        media::mojom::VideoCaptureState::FAILED);
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
        media::mojom::VideoCaptureState::ENDED);
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
          media::mojom::VideoCaptureState::FAILED);
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
  // base::Unretained() usage is safe because |render_process_host_delegate_|
  // is destroyed on UI thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RenderProcessHostDelegate::NotifyStreamAdded,
                     base::Unretained(render_process_host_delegate_.get())));
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
  // base::Unretained() usage is safe because |render_process_host_delegate_| is
  // destroyed on UI thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RenderProcessHostDelegate::NotifyStreamRemoved,
                     base::Unretained(render_process_host_delegate_.get())));
}

void VideoCaptureHost::NotifyAllStreamsRemoved() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  while (number_of_active_streams_ > 0)
    NotifyStreamRemoved();
}

}  // namespace content
