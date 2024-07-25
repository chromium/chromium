// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphics_shared_image_interface_provider_impl.h"

#include "base/task/bind_post_task.h"
#include "base/threading/platform_thread.h"
#include "gpu/ipc/client/client_shared_image_interface.h"

namespace content {

WebGraphicsSharedImageInterfaceProviderImpl::
    WebGraphicsSharedImageInterfaceProviderImpl(
        scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface)
    : shared_image_interface_(std::move(shared_image_interface)) {
  DCHECK(shared_image_interface_);

  task_gpu_channel_lost_on_main_thread_ = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&WebGraphicsSharedImageInterfaceProviderImpl::
                         GpuChannelLostOnMainThread,
                     weak_ptr_factory_.GetWeakPtr()));

  shared_image_interface_->gpu_channel()->AddObserver(this);
}

WebGraphicsSharedImageInterfaceProviderImpl::
    ~WebGraphicsSharedImageInterfaceProviderImpl() {
  shared_image_interface_->gpu_channel()->RemoveObserver(this);
}

void WebGraphicsSharedImageInterfaceProviderImpl::SetLostGpuChannelCallback(
    base::RepeatingClosure task) {
  gpu_channel_lost_callback_ = std::move(task);
}

gpu::SharedImageInterface*
WebGraphicsSharedImageInterfaceProviderImpl::SharedImageInterface() {
  return shared_image_interface_.get();
}

base::WeakPtr<blink::WebGraphicsSharedImageInterfaceProvider>
WebGraphicsSharedImageInterfaceProviderImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebGraphicsSharedImageInterfaceProviderImpl::OnGpuChannelLost() {
  // OnGpuChannelLost() is called on the IOThread. so it has to be forwareded
  // to the CrRendererMain thread.
  if (task_gpu_channel_lost_on_main_thread_) {
    std::move(task_gpu_channel_lost_on_main_thread_).Run();
  }
}

void WebGraphicsSharedImageInterfaceProviderImpl::GpuChannelLostOnMainThread() {
  if (!shared_image_interface_) {
    return;
  }

  shared_image_interface_.reset();

  if (!gpu_channel_lost_callback_.is_null()) {
    gpu_channel_lost_callback_.Run();
  }
}

}  // namespace content
