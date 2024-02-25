// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_HOST_H_
#define CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_HOST_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "base/win/windows_types.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class UnguessableToken;
}

namespace gfx {
class Size;
}

namespace gpu {
class GpuChannelHost;
struct Mailbox;
}  // namespace gpu

namespace content {

// Handles all mojo calls between DCOMPTextureWrapperImpl and the GPU process.
class DCOMPTextureHost : public gpu::mojom::DCOMPTextureClient {
 public:
  // Listener class that is listening to the DCOMP texture updates.
  class Listener {
   public:
    virtual void OnSharedImageMailboxBound(gpu::Mailbox mailbox) = 0;
    virtual void OnOutputRectChange(gfx::Rect output_rect) = 0;
  };

  DCOMPTextureHost(
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t route_id,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      mojo::PendingAssociatedRemote<gpu::mojom::DCOMPTexture> texture,
      Listener* listener);
  DCOMPTextureHost(const DCOMPTextureHost&) = delete;
  DCOMPTextureHost& operator=(const DCOMPTextureHost&) = delete;
  ~DCOMPTextureHost() override;

  void SetTextureSize(const gfx::Size& size);

  using SetDCOMPSurfaceHandleCB = base::OnceCallback<void(bool)>;
  void SetDCOMPSurfaceHandle(
      const base::UnguessableToken& token,
      SetDCOMPSurfaceHandleCB set_dcomp_surface_handle_cb);

 private:
  void OnDisconnectedFromGpuProcess();

  // gpu::mojom::DCOMPTextureClient:
  void OnSharedImageMailboxBound(const gpu::Mailbox& mailbox) override;
  void OnOutputRectChange(const gfx::Rect& output_rect) override;

  scoped_refptr<gpu::GpuChannelHost> channel_;
  const int32_t route_id_;
  const raw_ptr<Listener>
      listener_;  // Raw pointer is safe because the `listener_`
                  // (DCOMPTextureWrapperImpl) owns `this`.

  // Calls into the DCOMPTexture in the GPU process.
  mojo::AssociatedRemote<gpu::mojom::DCOMPTexture> texture_remote_;

  // Receives calls from the DCOMPTexture instance in the GPU process.
  mojo::AssociatedReceiver<gpu::mojom::DCOMPTextureClient> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_HOST_H_
