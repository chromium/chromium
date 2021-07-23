// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_WRAPPER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WIN_DCOMP_TEXTURE_WRAPPER_IMPL_H_

#include "base/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/renderer/media/win/dcomp_texture_factory.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/video_frame.h"
#include "media/base/win/dcomp_texture_wrapper.h"
#include "ui/gfx/geometry/rect.h"

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
// - When the DCOMPTexture's OnDCOMPSurfaceHandleBound() callback is fired, we
//   notify MediaFoundationRendererClient that the DCOMP handle has been bound,
//   via the `dcomp_surface_handle_bound_cb_` callback.
class DCOMPTextureWrapperImpl : public media::DCOMPTextureWrapper,
                                public DCOMPTextureHost::Listener {
 public:
  static std::unique_ptr<media::DCOMPTextureWrapper> Create(
      scoped_refptr<DCOMPTextureFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);

  ~DCOMPTextureWrapperImpl() override;

  // Initializes `this` and run `init_cb` with success or failure. All other
  // methods should only be called after a successful initialization.
  void Initialize(const gfx::Size& natural_size,
                  DCOMPSurfaceHandleBoundCB dcomp_surface_handle_bound_cb,
                  CompositionParamsReceivedCB comp_params_received_cb,
                  InitCB init_cb) override;

  // DCOMPTextureWrapper:
  void UpdateTextureSize(const gfx::Size& natural_size) override;
  void SetDCOMPSurface(const base::UnguessableToken& surface_token) override;
  void CreateVideoFrame(CreateVideoFrameCB create_video_frame_cb) override;

 private:
  DCOMPTextureWrapperImpl(
      scoped_refptr<DCOMPTextureFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  DCOMPTextureWrapperImpl(const DCOMPTextureWrapperImpl&) = delete;
  DCOMPTextureWrapperImpl& operator=(const DCOMPTextureWrapperImpl&) = delete;

  // DCOMPTextureHost::Listener:
  void OnSharedImageMailboxBound(gpu::Mailbox mailbox) override;
  void OnDCOMPSurfaceHandleBound(bool success) override;
  void OnCompositionParamsReceived(gfx::Rect output_rect) override;

  scoped_refptr<DCOMPTextureFactory> factory_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  gfx::Size natural_size_;  // Size of the video frames.
  DCOMPSurfaceHandleBoundCB dcomp_surface_handle_bound_cb_;
  CompositionParamsReceivedCB comp_params_received_cb_;

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
