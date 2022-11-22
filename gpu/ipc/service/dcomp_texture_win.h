// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_
#define GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gl/dcomp_surface_proxy.h"

namespace gfx {
class Size;
}

namespace gpu {
class GpuChannel;
struct Mailbox;

class DCOMPTexture : public gl::DCOMPSurfaceProxy,
                     public SharedContextState::ContextLostObserver,
                     public base::PowerSuspendObserver,
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

  // gl::DCOMPSurfaceProxy implementation.
  const gfx::Size& GetSize() const override;
  HANDLE GetSurfaceHandle() override;
  void SetParentWindow(HWND parent) override;
  void SetRect(const gfx::Rect& window_relative_rect) override;
  void SetProtectedVideoType(
      gfx::ProtectedVideoType protected_video_type) override;

 private:
  DCOMPTexture(GpuChannel* channel,
               int32_t route_id,
               mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
               scoped_refptr<SharedContextState> context_state);
  ~DCOMPTexture() override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

  // base::PowerSuspendObserver implementation.
  void OnResume() override;

  // mojom::DCOMPTexture implementation.
  void StartListening(
      mojo::PendingAssociatedRemote<mojom::DCOMPTextureClient> client) override;
  void SetTextureSize(const gfx::Size& size) override;
  void SetDCOMPSurfaceHandle(const base::UnguessableToken& token,
                             SetDCOMPSurfaceHandleCallback callback) override;

  gpu::Mailbox CreateSharedImage();
  gfx::Rect GetParentWindowRect();

  void OnUpdateParentWindowRect();
  void SendOutputRect();
  void ResetSizeIfNeeded();

  // Size of {1, 1} to signify the Media Foundation rendering pipeline is not
  // ready to setup DCOMP video yet, or should not display due to hardware
  // context reset.
  gfx::Size size_ = gfx::Size(1, 1);

  base::win::ScopedHandle surface_handle_;
  HWND last_parent_ = nullptr;
  gfx::ProtectedVideoType protected_video_type_ =
      gfx::ProtectedVideoType::kClear;

  bool shared_image_mailbox_created_ = false;
  raw_ptr<GpuChannel> channel_ = nullptr;
  const int32_t route_id_;
  scoped_refptr<SharedContextState> context_state_;
  SequenceId sequence_;

  gfx::Rect last_output_rect_;
  gfx::Rect parent_window_rect_;
  gfx::Rect window_relative_rect_;
  base::RepeatingTimer window_pos_timer_;

  // Last time when a power resume or GPU change happened. This is used to
  // decide whether there is a risk that hardware context reset happened and we
  // should release dcomp surface.
  base::TimeTicks last_power_change_time_;

  mojo::AssociatedReceiver<mojom::DCOMPTexture> receiver_;
  mojo::AssociatedRemote<mojom::DCOMPTextureClient> client_;

  base::WeakPtrFactory<DCOMPTexture> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_DCOMP_TEXTURE_WIN_H_
