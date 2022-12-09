// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"

namespace media {

class PictureBuffer;
class VaapiWrapper;

using CreatePictureCB = base::RepeatingCallback<std::unique_ptr<VaapiPicture>(
    scoped_refptr<VaapiWrapper>,
    const MakeGLContextCurrentCallback&,
    const BindGLImageCallback&,
    const PictureBuffer&,
    const gfx::Size&,
    uint32_t,
    uint32_t)>;

// Factory of platform dependent VaapiPictures.
class MEDIA_GPU_EXPORT VaapiPictureFactory {
 public:
  enum VaapiImplementation {
    kVaapiImplementationNone = 0,
    kVaapiImplementationDrm,
    kVaapiImplementationAngle,
  };

  VaapiPictureFactory();

  VaapiPictureFactory(const VaapiPictureFactory&) = delete;
  VaapiPictureFactory& operator=(const VaapiPictureFactory&) = delete;

  virtual ~VaapiPictureFactory();

  // Creates a VaapiPicture of picture_buffer.size() associated with
  // picture_buffer.id().
  virtual std::unique_ptr<VaapiPicture> Create(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const PictureBuffer& picture_buffer,
      const gfx::Size& visible_size);

  // Return the type of the VaapiPicture implementation for the given GL
  // implementation.
  VaapiImplementation GetVaapiImplementation(gl::GLImplementation gl_impl);

  // Determines whether the DownloadFromSurface() method of the VaapiPictures
  // created by this factory requires a processing pipeline VaapiWrapper.
  bool NeedsProcessingPipelineForDownloading() const;

  // Gets the texture target used to bind EGLImages (either GL_TEXTURE_2D on X11
  // or GL_TEXTURE_EXTERNAL_OES on DRM).
  uint32_t GetGLTextureTarget();

  // Buffer format to use for output buffers backing PictureBuffers. This is
  // the format decoded frames in VASurfaces are converted into.
  gfx::BufferFormat GetBufferFormat();

  std::unique_ptr<VaapiPicture> CreateVaapiPictureNative(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const PictureBuffer& picture_buffer,
      const gfx::Size& visible_size,
      uint32_t client_texture_id,
      uint32_t service_texture_id);

  std::map<gl::GLImplementation, VaapiPictureFactory::VaapiImplementation>
      vaapi_impl_pairs_;

 private:
  void DeterminePictureCreationAndDownloadingMechanism();

  CreatePictureCB create_picture_cb_;
  bool needs_vpp_for_downloading_ = false;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_FACTORY_H_
