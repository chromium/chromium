// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
// The use of Unretained:
//
// We make sure deletion is the last task on the IO thread for a
// VideoCaptureImpl object. This allows the use of Unretained() binding.

#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

media::VideoCaptureFormats ToVideoCaptureFormats(
    const Vector<media::VideoCaptureFormat>& format_vector) {
  media::VideoCaptureFormats formats;
  std::copy(format_vector.begin(), format_vector.end(),
            std::back_inserter(formats));
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
      render_main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
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
  devices_.Clear();
}

base::OnceClosure WebVideoCaptureImplManager::UseDevice(
    const media::VideoCaptureSessionId& id) {
  DVLOG(1) << __func__ << " session id: " << id;
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  if (it == devices_.end()) {
    devices_.emplace_back(DeviceEntry());
    it = devices_.end() - 1;
    it->session_id = id;
    it->impl = CreateVideoCaptureImplForTesting(id);
    if (!it->impl)
      it->impl.reset(new VideoCaptureImpl(id));
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
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());

  // This ID is used to identify a client of VideoCaptureImpl.
  const int client_id = ++next_client_id_;

  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::StartCapture,
                                base::Unretained(it->impl.get()), client_id,
                                params, state_update_cb, deliver_frame_cb));
  return base::BindOnce(&WebVideoCaptureImplManager::StopCapture,
                        weak_factory_.GetWeakPtr(), client_id, id);
}

void WebVideoCaptureImplManager::RequestRefreshFrame(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::RequestRefreshFrame,
                                base::Unretained(it->impl.get())));
}

void WebVideoCaptureImplManager::Suspend(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  if (it->is_individually_suspended)
    return;  // Device has already been individually suspended.
  if (it->client_count > 1)
    return;  // Punt when there is >1 client (see comments in UseDevice()).
  it->is_individually_suspended = true;
  if (is_suspending_all_)
    return;  // Device should already be suspended.
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                base::Unretained(it->impl.get()), true));
}

void WebVideoCaptureImplManager::Resume(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  if (!it->is_individually_suspended)
    return;  // Device was not individually suspended.
  it->is_individually_suspended = false;
  if (is_suspending_all_)
    return;  // Device must remain suspended until all are resumed.
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                base::Unretained(it->impl.get()), false));
}

void WebVideoCaptureImplManager::GetDeviceSupportedFormats(
    const media::VideoCaptureSessionId& id,
    VideoCaptureDeviceFormatsCB callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::GetDeviceSupportedFormats,
                                base::Unretained(it->impl.get()),
                                base::BindOnce(&MediaCallbackCaller,
                                               std::move(callback))));
}

void WebVideoCaptureImplManager::GetDeviceFormatsInUse(
    const media::VideoCaptureSessionId& id,
    VideoCaptureDeviceFormatsCB callback) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::GetDeviceFormatsInUse,
                                base::Unretained(it->impl.get()),
                                base::BindOnce(&MediaCallbackCaller,
                                               std::move(callback))));
}

std::unique_ptr<VideoCaptureImpl>
WebVideoCaptureImplManager::CreateVideoCaptureImplForTesting(
    const media::VideoCaptureSessionId& session_id) const {
  return nullptr;
}

void WebVideoCaptureImplManager::StopCapture(
    int client_id,
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::StopCapture,
                                base::Unretained(it->impl.get()), client_id));
}

void WebVideoCaptureImplManager::UnrefDevice(
    const media::VideoCaptureSessionId& id) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
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
    const auto it = std::find_if(
        devices_.begin(), devices_.end(),
        [id](const DeviceEntry& entry) { return entry.session_id == id; });
    DCHECK(it != devices_.end());
    if (it->is_individually_suspended)
      continue;  // Either: 1) Already suspended; or 2) Should not be resumed.
    // Use of base::Unretained() is safe because |devices_| is released on the
    // |io_task_runner()| as well.
    Platform::Current()->GetIOTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoCaptureImpl::SuspendCapture,
                                  base::Unretained(it->impl.get()), suspend));
  }
}

void WebVideoCaptureImplManager::OnFrameDropped(
    const media::VideoCaptureSessionId& id,
    media::VideoCaptureFrameDropReason reason) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureImpl::OnFrameDropped,
                                base::Unretained(it->impl.get()), reason));
}

void WebVideoCaptureImplManager::OnLog(const media::VideoCaptureSessionId& id,
                                       const WebString& message) {
  DCHECK(render_main_task_runner_->BelongsToCurrentThread());
  const auto it = std::find_if(
      devices_.begin(), devices_.end(),
      [id](const DeviceEntry& entry) { return entry.session_id == id; });
  DCHECK(it != devices_.end());
  // Use of base::Unretained() is safe because |devices_| is released on the
  // |io_task_runner()| as well.
  PostCrossThreadTask(*Platform::Current()->GetIOTaskRunner().get(), FROM_HERE,
                      CrossThreadBindOnce(&VideoCaptureImpl::OnLog,
                                          CrossThreadUnretained(it->impl.get()),
                                          String(message)));
}

}  // namespace blink
