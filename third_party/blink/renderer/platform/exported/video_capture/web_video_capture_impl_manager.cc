// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes about interactions with VideoCaptureImpl.
//
// How is VideoCaptureImpl used:
//
// VideoCaptureImpl is an IO thread object while WebVideoCaptureImplManager
// lives only on the render thread. It is only possible to access an
// object of VideoCaptureImpl via a task on the IO thread.
//
// How is VideoCaptureImpl deleted:
//
// A task is posted to the IO thread to delete a VideoCaptureImpl.
// Immediately after that the pointer to it is dropped. This means no
// access to this VideoCaptureImpl object is possible on the render
// thread. Also note that VideoCaptureImpl does not post task to itself.
//

#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

media::VideoCaptureFormats ToVideoCaptureFormats(
    const Vector<media::VideoCaptureFormat>& format_vector) {
  media::VideoCaptureFormats formats;
  base::ranges::copy(format_vector, std::back_inserter(formats));
  return formats;
}

void MediaCallbackCaller(VideoCaptureDeviceFormatsCB media_callback,
                         const Vector<media::VideoCaptureFormat>& formats) {
  std::move(media_callback).Run(ToVideoCaptureFormats(formats));
}

struct WebVideoCaptureImplManager::DeviceEntry {
  media::VideoCaptureSessionId session_id;

  // To be used and destroyed only on the IO thread.
  std::unique_ptr<VideoCaptureImpl> impl;

  // Number of clients using |impl|.
  int client_count;

  // This is set to true if this device is being suspended, via
  // WebVideoCaptureImplManager::Suspend().
  // See also: WebVideoCaptureImplManager::is_suspending_all_.
  bool is_individually_suspended;

  DeviceEntry() : client_count(0), is_individually_suspended(false) {}
  DeviceEntry(DeviceEntry&& other) = default;
  DeviceEntry& operator=(DeviceEntry&& other) = default;
  ~DeviceEntry() = default;
};

WebVideoCaptureImplManager::WebVideoCaptureImplManager()
    : next_client_id_(0),
      render_main_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      is_suspending_all_(false) {}

WebVideoCaptureImplManager::~WebVideoCaptureImplManager() {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  if (devices_.empty())
    return;
  // Forcibly release all video capture resources.
  for (auto& entry : devices_) {
    Platform::Current()->GetIOTaskRunner()->DeleteSoon(FROM_HERE,
                                                       entry.impl.release());
  }
  devices_.clear();
}

base::OnceClosure WebVideoCaptureImplManager::UseDevice(
    const media::VideoCaptureSessionId& id,
    const BrowserInterfaceBrokerProxy& browser_interface_broker) {
  DVLOG(1) << __func__ << " session id: " << id;
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end()) {
    devices_.emplace_back(DeviceEntry());
    it = devices_.end() - 1;
    it->session_id = id;
    it->impl = CreateVideoCaptureImpl(id, browser_interface_broker);
  }
  ++it->client_count;

  // Design limit: When there are multiple clients, WebVideoCaptureImplManager
  // would have to individually track which ones requested suspending/resuming,
  // in order to determine whether the whole device should be suspended.
  // Instead, handle the non-common use case of multiple clients by just
  // resuming the suspended device, and disable suspend functionality while
  // there are multiple clients.
  if (it->is_individually_suspended)
    Resume(id);

  return base::BindOnce(&WebVideoCaptureImplManager::UnrefDevice,
                        weak_factory_.GetWeakPtr(), id);
}

base::OnceClosure WebVideoCaptureImplManager::StartCapture(
    const media::VideoCaptureSessionId& id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb,
    const VideoCaptureSubCaptureTargetVersionCB& sub_capture_target_version_cb,
    const VideoCaptureNotifyFrameDroppedCB& frame_dropped_cb) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return base::OnceClosure();

  // This ID is used to identify a client of VideoCaptureImpl.
  const int client_id = ++next_client_id_;

  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureImpl::StartCapture, it->impl->GetWeakPtr(),
                     client_id, params, state_update_cb, deliver_frame_cb,
                     sub_capture_target_version_cb, frame_dropped_cb));
  return base::BindOnce(&WebVideoCaptureImplManager::StopCapture,
                        weak_factory_.GetWeakPtr(), client_id, id);
}

void WebVideoCaptureImplManager::RequestRefreshFrame(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::RequestRefreshFrame,
                                it->impl->GetWeakPtr()));
}

void WebVideoCaptureImplManager::Suspend(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  if (it->is_individually_suspended)
    return;  // Device has already been individually suspended.
  if (it->client_count > 1)
    return;  // Punt when there is >1 client (see comments in UseDevice()).
  it->is_individually_suspended = true;
  if (is_suspending_all_)
    return;  // Device should already be suspended.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                it->impl->GetWeakPtr(), true));
}

void WebVideoCaptureImplManager::Resume(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  if (!it->is_individually_suspended)
    return;  // Device was not individually suspended.
  it->is_individually_suspended = false;
  if (is_suspending_all_)
    return;  // Device must remain suspended until all are resumed.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                it->impl->GetWeakPtr(), false));
}

void WebVideoCaptureImplManager::GetDeviceSupportedFormats(
    const media::VideoCaptureSessionId& id,
    VideoCaptureDeviceFormatsCB callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoCaptureImpl::GetDeviceSupportedFormats, it->impl->GetWeakPtr(),
          base::BindOnce(&MediaCallbackCaller, std::move(callback))));
}

void WebVideoCaptureImplManager::GetDeviceFormatsInUse(
    const media::VideoCaptureSessionId& id,
    VideoCaptureDeviceFormatsCB callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoCaptureImpl::GetDeviceFormatsInUse, it->impl->GetWeakPtr(),
          base::BindOnce(&MediaCallbackCaller, std::move(callback))));
}

std::unique_ptr<VideoCaptureImpl>
WebVideoCaptureImplManager::CreateVideoCaptureImpl(
    const media::VideoCaptureSessionId& session_id,
    const BrowserInterfaceBrokerProxy& browser_interface_broker) const {
  return std::make_unique<VideoCaptureImpl>(
      session_id, render_main_task_runner_, browser_interface_broker);
}

void WebVideoCaptureImplManager::StopCapture(
    int client_id,
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::StopCapture,
                                it->impl->GetWeakPtr(), client_id));
}

void WebVideoCaptureImplManager::UnrefDevice(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  // Unlike other methods, this is where the device is deleted, so it must still
  // exist.
  CHECK(it != devices_.end());
  DCHECK_GT(it->client_count, 0);
  --it->client_count;
  if (it->client_count > 0)
    return;
  Platform::Current()->GetIOTaskRunner()->DeleteSoon(FROM_HERE,
                                                     it->impl.release());

  size_t index = std::distance(devices_.begin(), it);
  devices_.EraseAt(index);
}

void WebVideoCaptureImplManager::SuspendDevices(
    const MediaStreamDevices& video_devices,
    bool suspend) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  if (is_suspending_all_ == suspend)
    return;
  is_suspending_all_ = suspend;
  for (const MediaStreamDevice& device : video_devices) {
    const media::VideoCaptureSessionId id = device.session_id();
    const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
    if (it == devices_.end())
      return;
    if (it->is_individually_suspended)
      continue;  // Either: 1) Already suspended; or 2) Should not be resumed.
    Platform::Current()->GetIOTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                  it->impl->GetWeakPtr(), suspend));
  }
}

void WebVideoCaptureImplManager::OnLog(const media::VideoCaptureSessionId& id,
                                       const WebString& message) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;

  PostCrossThreadTask(
      *Platform::Current()->GetIOTaskRunner().get(), FROM_HERE,
      CrossThreadBindOnce(&VideoCaptureImpl::OnLog, it->impl->GetWeakPtr(),
                          String(message)));
}

VideoCaptureFeedbackCB WebVideoCaptureImplManager::GetFeedbackCallback(
    const media::VideoCaptureSessionId& id) const {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  return base::BindRepeating(
      &WebVideoCaptureImplManager::ProcessFeedback,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &WebVideoCaptureImplManager::ProcessFeedbackInternal,
          weak_factory_.GetMutableWeakPtr(), id)));
}

// static
void WebVideoCaptureImplManager::ProcessFeedback(
    VideoCaptureFeedbackCB callback_to_io_thread,
    const media::VideoCaptureFeedback& feedback) {
  // process feedback can be called on any thread by the client.
  callback_to_io_thread.Run(feedback);
}

void WebVideoCaptureImplManager::ProcessFeedbackInternal(
    const media::VideoCaptureSessionId& id,
    const media::VideoCaptureFeedback& feedback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = base::ranges::find(devices_, id, &DeviceEntry::session_id);
  if (it == devices_.end())
    return;
  PostCrossThreadTask(*Platform::Current()->GetIOTaskRunner().get(), FROM_HERE,
                      CrossThreadBindOnce(&VideoCaptureImpl::ProcessFeedback,
                                          it->impl->GetWeakPtr(), feedback));
}

}  // namespace blink
