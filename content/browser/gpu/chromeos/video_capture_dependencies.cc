// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/chromeos/video_capture_dependencies.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
void VideoCaptureDependencies::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        accelerator) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&VideoCaptureDependencies::CreateJpegDecodeAccelerator,
                       std::move(accelerator)));
    return;
  }

  auto* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, true /*force_create*/);
  if (host) {
    host->gpu_service()->CreateJpegDecodeAccelerator(std::move(accelerator));
  } else {
    LOG(ERROR) << "No GpuProcessHost";
  }
}

#if defined(OS_CHROMEOS)
// static
void VideoCaptureDependencies::CreateJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        accelerator) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&VideoCaptureDependencies::CreateJpegEncodeAccelerator,
                       std::move(accelerator)));
    return;
  }

  auto* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, true /*force_create*/);
  if (host) {
    host->gpu_service()->CreateJpegEncodeAccelerator(std::move(accelerator));
  } else {
    LOG(ERROR) << "No GpuProcessHost";
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace content
