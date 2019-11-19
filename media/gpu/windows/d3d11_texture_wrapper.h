// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

using CommandBufferHelperPtr = scoped_refptr<CommandBufferHelper>;
using MailboxHolderArray = gpu::MailboxHolder[VideoFrame::kMaxPlanes];
using GetCommandBufferHelperCB =
    base::RepeatingCallback<CommandBufferHelperPtr()>;

class D3D11PictureBuffer;

// Support different strategies for processing pictures - some may need copying,
// for example.
class MEDIA_GPU_EXPORT Texture2DWrapper {
 public:
  Texture2DWrapper(ComD3D11Texture2D texture);
  virtual ~Texture2DWrapper();

  virtual const ComD3D11Texture2D Texture() const;

  // This pointer can be raw, since each Texture2DWrapper is directly owned
  // by the D3D11PictureBuffer through a unique_ptr.
  virtual bool ProcessTexture(const D3D11PictureBuffer* owner_pb,
                              MailboxHolderArray* mailbox_dest) = 0;

  // |array_slice| Tells us which array index of the array-type Texture2D
  // we should be using - if it is not an array-type, |array_slice| is 0.
  virtual bool Init(GetCommandBufferHelperCB get_helper_cb,
                    size_t array_slice,
                    gfx::Size size) = 0;

 private:
  ComD3D11Texture2D texture_;
};

// The default texture wrapper that uses GPUResources to talk to hardware
// on behalf of a Texture2D.
class MEDIA_GPU_EXPORT DefaultTexture2DWrapper : public Texture2DWrapper {
 public:
  DefaultTexture2DWrapper(ComD3D11Texture2D texture);
  ~DefaultTexture2DWrapper() override;

  bool Init(GetCommandBufferHelperCB get_helper_cb,
            size_t array_slice,
            gfx::Size size) override;

  bool ProcessTexture(const D3D11PictureBuffer* owner_pb,
                      MailboxHolderArray* mailbox_dest) override;

 private:
  // Things that are to be accessed / freed only on the main thread.  In
  // addition to setting up the textures to render from a D3D11 texture,
  // these also hold the chrome GL Texture objects so that the client
  // can use the mailbox.
  class GpuResources {
   public:
    GpuResources();
    ~GpuResources();

    bool Init(GetCommandBufferHelperCB get_helper_cb,
              int array_slice,
              const std::vector<gpu::Mailbox> mailboxes,
              GLenum target,
              gfx::Size size,
              ComD3D11Texture2D angle_texture,
              int textures_per_picture);

    std::vector<uint32_t> service_ids_;

   private:
    scoped_refptr<CommandBufferHelper> helper_;

    DISALLOW_COPY_AND_ASSIGN(GpuResources);
  };

  std::unique_ptr<GpuResources> gpu_resources_;
  MailboxHolderArray mailbox_holders_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
