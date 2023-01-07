// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_SERVICE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_SERVICE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace gpu {
class GpuChannelHost;
}  // namespace gpu

namespace content {

class OverlayStateServiceProvider {
 public:
  virtual bool RegisterObserver(
      mojo::PendingRemote<gpu::mojom::OverlayStateObserver> pending_remote,
      const gpu::Mailbox& mailbox) = 0;
  virtual ~OverlayStateServiceProvider() = default;
};

// Wrapper class for getting an OverlayStateService remote in the GPU
// process.
class OverlayStateServiceProviderImpl : public OverlayStateServiceProvider {
 public:
  explicit OverlayStateServiceProviderImpl(
      scoped_refptr<gpu::GpuChannelHost> channel);
  ~OverlayStateServiceProviderImpl() override;

  bool RegisterObserver(
      mojo::PendingRemote<gpu::mojom::OverlayStateObserver> pending_remote,
      const gpu::Mailbox& mailbox) override;

  // Returns true if the gpu channel is lost.
  bool IsLost() const;

 private:
  OverlayStateServiceProviderImpl(const OverlayStateServiceProviderImpl&) =
      delete;
  OverlayStateServiceProviderImpl& operator=(
      const OverlayStateServiceProviderImpl&) = delete;

  scoped_refptr<gpu::GpuChannelHost> channel_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_SERVICE_PROVIDER_H_
