// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_
#define GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gl/gl_image_dcomp_surface.h"

namespace gfx {
class Size;
}

namespace gpu {
class GpuChannel;
struct Mailbox;

class DCOMPTexture : public gl::GLImageDCOMPSurface,
                     public SharedContextState::ContextLostObserver,
                     public mojom::DCOMPTexture {
 public:
  // A nullptr is returned if it fails to create one.
  static scoped_refptr<DCOMPTexture> Create(
      GpuChannel* channel,
      int route_id,
      mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver);

  // Cleans up related data and nulls |channel_|. Called when the channel
  // releases its ref on this class.
  void ReleaseChannel();

 private:
  DCOMPTexture(GpuChannel* channel,
               int32_t route_id,
               mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
               scoped_refptr<SharedContextState> context_state);
  DCOMPTexture(const DCOMPTexture&) = delete;
  DCOMPTexture& operator=(const DCOMPTexture&) = delete;
  ~DCOMPTexture() override;

  // gl::GLImage implementation is in gl::GLImageDCOMPSurface.

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

  // mojom::DCOMPTexture:
  void StartListening(
      mojo::PendingAssociatedRemote<mojom::DCOMPTextureClient> client) override;
  void SetTextureSize(const gfx::Size& size) override;
  void SetDCOMPSurfaceHandle(const base::UnguessableToken& token,
                             SetDCOMPSurfaceHandleCallback callback) override;

  gpu::Mailbox CreateSharedImage();
  gfx::Rect GetParentWindowRect();
  void SetParentWindow(HWND parent) override;
  void OnUpdateParentWindowRect();
  void SetRect(const gfx::Rect& window_relative_rect) override;
  void SendOutputRect();
  bool IsContextValid() const override;

  bool context_lost_ = false;
  bool shared_image_mailbox_created_ = false;
  raw_ptr<GpuChannel> channel_ = nullptr;
  const int32_t route_id_;
  scoped_refptr<SharedContextState> context_state_;
  SequenceId sequence_;

  mojo::AssociatedReceiver<mojom::DCOMPTexture> receiver_;
  mojo::AssociatedRemote<mojom::DCOMPTextureClient> client_;

  gfx::Rect last_output_rect_;
  gfx::Rect parent_window_rect_;
  gfx::Rect window_relative_rect_;
  base::RepeatingTimer window_pos_timer_;

  base::WeakPtrFactory<DCOMPTexture> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_
