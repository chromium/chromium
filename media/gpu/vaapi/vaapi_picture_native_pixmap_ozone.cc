// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_ozone.h"

#include "gpu/command_buffer/service/shared_image/gl_image_native_pixmap.h"
#include "media/base/format_utils.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_status.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace media {

VaapiPictureNativePixmapOzone::VaapiPictureNativePixmapOzone(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    const gfx::Size& visible_size,
    uint32_t texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPictureNativePixmap(std::move(vaapi_wrapper),
                               make_context_current_cb,
                               bind_image_cb,
                               picture_buffer_id,
                               size,
                               visible_size,
                               texture_id,
                               client_texture_id,
                               texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either |texture_id| and |client_texture_id| are both zero, or not.
  DCHECK((texture_id == 0 && client_texture_id == 0) ||
         (texture_id != 0 && client_texture_id != 0));
}

VaapiPictureNativePixmapOzone::~VaapiPictureNativePixmapOzone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  // Reset |va_surface_| before |gl_image_| to preserve the order of destruction
  // before the refactoring done in
  // https://chromium-review.googlesource.com/c/chromium/src/+/4025005.
  // TODO(crbug.com/1366367): Determine whether preserving this order matters
  // and remove these calls if not.
  va_surface_.reset();
  gl_image_.reset();
}

VaapiStatus VaapiPictureNativePixmapOzone::Initialize(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixmap);
  DCHECK(pixmap->AreDmaBufFdsValid());
  // Create a |va_surface_| from dmabuf fds (pixmap->GetDmaBufFd)
  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(pixmap);
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return VaapiStatus::Codes::kNoSurface;
  }

  // ARC++ has no texture ids.
  if (texture_id_ == 0 && client_texture_id_ == 0)
    return VaapiStatus::Codes::kOk;

  // Import dmabuf fds into the output gl texture through EGLImage.
  if (make_context_current_cb_ && !make_context_current_cb_.Run())
    return VaapiStatus::Codes::kBadContext;

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);

  const gfx::BufferFormat format = pixmap->GetBufferFormat();

  // TODO(b/220336463): plumb the right color space.
  auto image = gpu::GLImageNativePixmap::Create(
      visible_size_, format, std::move(pixmap),
      base::strict_cast<GLenum>(texture_target_),
      base::strict_cast<GLuint>(texture_id_));
  if (!image) {
    LOG(ERROR) << "Failed to create GLImage";
    return VaapiStatus::Codes::kFailedToInitializeImage;
  }

  gl_image_ = std::move(image);

  if (bind_image_cb_ &&
      !bind_image_cb_.Run(client_texture_id_, texture_target_, gl_image_)) {
    LOG(ERROR) << "Failed to bind client_texture_id";
    return VaapiStatus::Codes::kFailedToBindImage;
  }

  return VaapiStatus::Codes::kOk;
}

VaapiStatus VaapiPictureNativePixmapOzone::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  gfx::BufferUsage buffer_usage = gfx::BufferUsage::SCANOUT_VDA_WRITE;
#if BUILDFLAG(USE_VAAPI_X11)
  // The 'VaapiVideoDecodeAccelerator' requires the VPP to download the decoded
  // frame from the internal surface to the allocated native pixmap.
  // 'SCANOUT_VDA_WRITE' is used for 'YUV_420_BIPLANAR' on ChromeOS; For Linux,
  // the usage is set to 'GPU_READ' for 'RGBX_8888'.
  DCHECK(format == gfx::BufferFormat::RGBX_8888);
  buffer_usage = gfx::BufferUsage::GPU_READ;
#endif
  auto pixmap = factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, nullptr, size_, format, buffer_usage,
      /*framebuffer_size=*/visible_size_);
  if (!pixmap) {
    return VaapiStatus::Codes::kNoPixmap;
  }

  return Initialize(std::move(pixmap));
}

bool VaapiPictureNativePixmapOzone::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CanImportGpuMemoryBufferHandle(size_, format,
                                      gpu_memory_buffer_handle)) {
    VLOGF(1) << "Can't import the given GpuMemoryBufferHandle";
    return false;
  }

  const auto& plane = gpu_memory_buffer_handle.native_pixmap_handle.planes[0];
  if (size_.width() > static_cast<int>(plane.stride) ||
      size_.GetArea() > static_cast<int>(plane.size)) {
    DLOG(ERROR) << "GpuMemoryBufferHandle (stride=" << plane.stride
                << ", size=" << plane.size
                << "is smaller than size_=" << size_.ToString();
    return false;
  }

  // gfx::NativePixmapDmaBuf() will take ownership of the handle.
  return Initialize(
             base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
                 size_, format,
                 std::move(gpu_memory_buffer_handle.native_pixmap_handle)))
      .is_ok();
}

}  // namespace media
