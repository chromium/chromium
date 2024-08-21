// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgraphics_shared_image_interface_provider_impl.h"

#include "base/task/bind_post_task.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace blink {

// Created on the CrRendererMain or the DedicatedWorker thread.
WebGraphicsSharedImageInterfaceProviderImpl::
    WebGraphicsSharedImageInterfaceProviderImpl(
        scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface)
    : shared_image_interface_(std::move(shared_image_interface)) {
  DCHECK(shared_image_interface_);

  task_gpu_channel_lost_on_worker_thread_ = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&WebGraphicsSharedImageInterfaceProviderImpl::
                         GpuChannelLostOnWorkerThread,
                     weak_ptr_factory_.GetWeakPtr()));

  shared_image_interface_->gpu_channel()->AddObserver(this);
}

// Destroyed on the same ctor thread.
WebGraphicsSharedImageInterfaceProviderImpl::
    ~WebGraphicsSharedImageInterfaceProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Observers are automatically removed after channel lost notification.
  // Here only RemoveObserver when there is no gpu channel lost.
  if (shared_image_interface_) {
    shared_image_interface_->gpu_channel()->RemoveObserver(this);
  }
}

void WebGraphicsSharedImageInterfaceProviderImpl::AddGpuChannelLostObserver(
    BitmapGpuChannelLostObserver* ob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.push_back(ob);
}

void WebGraphicsSharedImageInterfaceProviderImpl::RemoveGpuChannelLostObserver(
    BitmapGpuChannelLostObserver* ob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Erase(observer_list_, ob);
}

gpu::SharedImageInterface*
WebGraphicsSharedImageInterfaceProviderImpl::SharedImageInterface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return shared_image_interface_.get();
}

base::WeakPtr<blink::WebGraphicsSharedImageInterfaceProvider>
WebGraphicsSharedImageInterfaceProviderImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void WebGraphicsSharedImageInterfaceProviderImpl::OnGpuChannelLost() {
  // OnGpuChannelLost() is called on the IOThread. so it has to be forwareded
  // to the thread where the provider is created.
  if (task_gpu_channel_lost_on_worker_thread_) {
    std::move(task_gpu_channel_lost_on_worker_thread_).Run();
  }
}

void WebGraphicsSharedImageInterfaceProviderImpl::
    GpuChannelLostOnWorkerThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!shared_image_interface_) {
    return;
  }
  shared_image_interface_.reset();

  for (BitmapGpuChannelLostObserver* observer : observer_list_) {
    observer->OnGpuChannelLost();
  }
}

}  // namespace blink
