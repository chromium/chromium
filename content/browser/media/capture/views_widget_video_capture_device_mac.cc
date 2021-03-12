// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/views_widget_video_capture_device_mac.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/remote_cocoa/browser/scoped_cg_window_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {

namespace {

void ResolveCGWindowIDToFrameSinkId(
    uint32_t cg_window_id,
    const base::WeakPtr<FrameSinkVideoCaptureDevice> device,
    const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* scoped_cg_window_id = remote_cocoa::ScopedCGWindowID::Get(cg_window_id);
  if (scoped_cg_window_id) {
    device_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device,
                       scoped_cg_window_id->GetFrameSinkId()));
  } else {
    // It is entirely possible (although unlikely) that the window corresponding
    // to |cg_window_id| be destroyed between when the capture source was
    // selected and when this code is executed. If that happens, the target is
    // lost.
    device_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                       device));
  }
}

}  // namespace

ViewsWidgetVideoCaptureDeviceMac::ViewsWidgetVideoCaptureDeviceMac(
    const DesktopMediaID& source_id)
    : weak_factory_(this) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ResolveCGWindowIDToFrameSinkId, source_id.id,
                                weak_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get()));
}

ViewsWidgetVideoCaptureDeviceMac::~ViewsWidgetVideoCaptureDeviceMac() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

}  // namespace content
