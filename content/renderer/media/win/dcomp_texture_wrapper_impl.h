// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_WRAPPER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_WRAPPER_IMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/renderer/media/win/dcomp_texture_factory.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/video_frame.h"
#include "media/base/win/dcomp_texture_wrapper.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace content {

class DCOMPTextureMailboxResources;

// Concrete implementation of DCOMPTextureWrapper. Created on the main
// thread but then lives on the media thread.
//
// The DCOMPTexture is an abstraction allowing Chrome to wrap a DCOMP surface
// living in the GPU process. It allows VideoFrames to be created from the
// DCOMP surface, in the Renderer process.
//
// The general idea behind our use of DCOMPTexture is as follows:
// - We request the creation of a DCOMPTexture in the GPU process via the
//   DCOMPTextureHost.
// - This class listens for callbacks from DCOMPTexture.
// - We create a SharedImage mailbox representing the DCOMPTexture at a given
//   size.
// - We create a VideoFrame which takes ownership of this SharedImage mailbox.
class CONTENT_EXPORT DCOMPTextureWrapperImpl
    : public media::DCOMPTextureWrapper,
      public DCOMPTextureHost::Listener {
 public:
  // Creates a media::DCOMPTextureWrapper implementation. Can return nullptr if
  // `factory` is null.
  static std::unique_ptr<media::DCOMPTextureWrapper> Create(
      scoped_refptr<DCOMPTextureFactory> factory,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);

  ~DCOMPTextureWrapperImpl() override;

  // Initializes `this` and returns success or failure. All other methods should
  // only be called after a successful initialization.
  bool Initialize(const gfx::Size& output_size,
                  OutputRectChangeCB output_rect_change_cb) override;

  // DCOMPTextureWrapper:
  void UpdateTextureSize(const gfx::Size& natural_size) override;
  void SetDCOMPSurfaceHandle(
      const base::UnguessableToken& token,
      SetDCOMPSurfaceHandleCB set_dcomp_surface_handle_cb) override;
  void CreateVideoFrame(const gfx::Size& natural_size,
                        CreateVideoFrameCB create_video_frame_cb) override;
  void CreateVideoFrame(const gfx::Size& natural_size,
                        gfx::GpuMemoryBufferHandle dx_handle,
                        CreateDXVideoFrameCB create_video_frame_cb) override;

 private:
  DCOMPTextureWrapperImpl(
      scoped_refptr<DCOMPTextureFactory> factory,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner);
  DCOMPTextureWrapperImpl(const DCOMPTextureWrapperImpl&) = delete;
  DCOMPTextureWrapperImpl& operator=(const DCOMPTextureWrapperImpl&) = delete;

  // DCOMPTextureHost::Listener:
  void OnSharedImageMailboxBound(gpu::Mailbox mailbox) override;
  void OnOutputRectChange(gfx::Rect output_rect) override;

  void OnDXVideoFrameDestruction(
      const gpu::SyncToken& sync_token,
      scoped_refptr<gpu::ClientSharedImage> shared_image);

  scoped_refptr<DCOMPTextureFactory> factory_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  gfx::Size natural_size_;  // Size of the video frames.
  gfx::Size output_size_;   // Size of the video output (on-screen size).
  OutputRectChangeCB output_rect_change_cb_;

  std::unique_ptr<DCOMPTextureHost> dcomp_texture_host_;

  bool mailbox_added_ = false;
  gpu::Mailbox mailbox_;

  CreateVideoFrameCB create_video_frame_cb_;

  // See .cc file for detailed comments.
  scoped_refptr<DCOMPTextureMailboxResources> dcomp_texture_resources_;

  base::WeakPtrFactory<DCOMPTextureWrapperImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_WRAPPER_IMPL_H_
