// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

class D3D11PictureBuffer;

using CommandBufferHelperPtr = scoped_refptr<CommandBufferHelper>;
using GetCommandBufferHelperCB =
    base::RepeatingCallback<CommandBufferHelperPtr()>;

// Support different strategies for processing pictures - some may need copying,
// for example.  Each wrapper owns the resources for a single texture, so it's
// up to you not to re-use a wrapper for a second image before a previously
// processed image is no longer needed.
class MEDIA_GPU_EXPORT Texture2DWrapper {
 public:
  using PictureBufferGPUResourceInitDoneCB =
      base::OnceCallback<void(scoped_refptr<media::D3D11PictureBuffer>)>;

  Texture2DWrapper();
  virtual ~Texture2DWrapper();

  // Initialize the wrapper.
  virtual D3D11Status Init(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferHelperCB get_helper_cb,
      ComD3D11Texture2D texture,
      size_t array_size,
      scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
      PictureBufferGPUResourceInitDoneCB
          picture_buffer_gpu_resource_init_done_cb) = 0;

  // "If the |texture| is shared and needs synchronization, it is important for
  // shared image to call begin access before any usage. This API is required to
  // be called:
  // - Before reading or writing to the texture via views on the texture or
  // other means.
  // - Before calling ProcessTexture.
  // And need reset the scoped access object to end access.
  virtual D3D11Status BeginSharedImageAccess() = 0;

  // Import |texture|, |array_slice| and return the shared image that can be
  // used to refer to it.
  virtual D3D11Status ProcessTexture(
      const gfx::ColorSpace& input_color_space,
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest_out) = 0;
};

// The default texture wrapper that uses GPUResources to talk to hardware
// on behalf of a Texture2D.  Each DefaultTexture2DWrapper owns GL textures
// that it uses to bind the provided input texture.  Thus, one needs one wrapper
// instance for each concurrently outstanding texture.
class MEDIA_GPU_EXPORT DefaultTexture2DWrapper : public Texture2DWrapper {
 public:
  // Error callback for GpuResource to notify us of errors.
  using OnErrorCB = base::OnceCallback<void(D3D11Status)>;

  // Callback for setting shared image representation and resume picture buffer
  // after gpu resource initialization.
  using GPUResourceInitCB =
      base::OnceCallback<void(scoped_refptr<media::D3D11PictureBuffer>,
                              std::unique_ptr<gpu::VideoImageRepresentation>,
                              scoped_refptr<gpu::ClientSharedImage>)>;

  // While the specific texture instance can change on every call to
  // ProcessTexture, the dxgi format must be the same for all of them.
  DefaultTexture2DWrapper(const gfx::Size& size,
                          const gfx::ColorSpace& color_space,
                          DXGI_FORMAT dxgi_format,
                          ComD3D11Device device);
  ~DefaultTexture2DWrapper() override;

  D3D11Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                   GetCommandBufferHelperCB get_helper_cb,
                   ComD3D11Texture2D in_texture,
                   size_t array_slice,
                   scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
                   Texture2DWrapper::PictureBufferGPUResourceInitDoneCB
                       picture_buffer_gpu_resource_init_done_cb) override;

  D3D11Status BeginSharedImageAccess() override;

  D3D11Status ProcessTexture(
      const gfx::ColorSpace& input_color_space,
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) override;

  void OnGPUResourceInitDone(
      scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
      std::unique_ptr<gpu::VideoImageRepresentation> shared_image_rep,
      scoped_refptr<gpu::ClientSharedImage> client_shared_image);

  ComD3D11Device GetVideoDevice() { return video_device_; }

 private:
  // Things that are to be accessed / freed only on the main thread.  In
  // addition to setting up the textures to render from a D3D11 texture,
  // these also hold the chrome GL Texture objects so that the client
  // can use the mailbox.
  class GpuResources {
   public:
    GpuResources(OnErrorCB on_error_cb,
                 GetCommandBufferHelperCB get_helper_cb,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 DXGI_FORMAT dxgi_format,
                 ComD3D11Device video_device,
                 ComD3D11Texture2D texture,
                 size_t array_slice,
                 scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
                 GPUResourceInitCB gpu_resource_init_cb);
    GpuResources(const GpuResources&) = delete;
    GpuResources& operator=(const GpuResources&) = delete;
    ~GpuResources();

   private:
    scoped_refptr<CommandBufferHelper> helper_;
    std::unique_ptr<gpu::SharedImageRepresentationFactoryRef> shared_image_;
    base::WeakPtrFactory<GpuResources> weak_factory_{this};
  };

  // Receive an error from |gpu_resources_| and store it in |received_error_|.
  void OnError(D3D11Status status);

  // The first error status that we've received from |gpu_resources_|, if any.
  std::optional<D3D11Status> received_error_;

  gfx::Size size_;
  gfx::ColorSpace color_space_;
  base::SequenceBound<GpuResources> gpu_resources_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  DXGI_FORMAT dxgi_format_;

  std::unique_ptr<gpu::VideoImageRepresentation> shared_image_rep_;
  std::unique_ptr<gpu::VideoImageRepresentation::ScopedWriteAccess>
      shared_image_access_;

  ComD3D11Device video_device_;

  Texture2DWrapper::PictureBufferGPUResourceInitDoneCB
      picture_buffer_gpu_resource_init_done_cb_;

  base::WeakPtrFactory<DefaultTexture2DWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_TEXTURE_WRAPPER_H_
