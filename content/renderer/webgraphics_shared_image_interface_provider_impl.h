// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBGRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_WEBGRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"

namespace content {

class WebGraphicsSharedImageInterfaceProviderImpl
    : public blink::WebGraphicsSharedImageInterfaceProvider,
      public gpu::GpuChannelLostObserver {
 public:
  explicit WebGraphicsSharedImageInterfaceProviderImpl(
      scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface);

  WebGraphicsSharedImageInterfaceProviderImpl(
      const WebGraphicsSharedImageInterfaceProviderImpl&) = delete;
  WebGraphicsSharedImageInterfaceProviderImpl& operator=(
      const WebGraphicsSharedImageInterfaceProviderImpl&) = delete;

  ~WebGraphicsSharedImageInterfaceProviderImpl() override;

  // WebGraphicsSharedImageInterfaceProvider implementation.
  void SetLostGpuChannelCallback(base::RepeatingClosure task) override;
  gpu::SharedImageInterface* SharedImageInterface() override;
  base::WeakPtr<blink::WebGraphicsSharedImageInterfaceProvider> GetWeakPtr()
      override;

  // gpu::GpuChannelLostObserver implementation.
  void OnGpuChannelLost() override;
  void GpuChannelLostOnMainThread();

 private:
  base::RepeatingClosure gpu_channel_lost_callback_;
  base::OnceCallback<void()> task_gpu_channel_lost_on_main_thread_;

  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  base::WeakPtrFactory<WebGraphicsSharedImageInterfaceProviderImpl>
      weak_ptr_factory_{this};
};

}  // namespace content
#endif  // CONTENT_RENDERER_WEBGRAPHICS_SHARED_IMAGE_INTERFACE_PROVIDER_IMPL_H_
