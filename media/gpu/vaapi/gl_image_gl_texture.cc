// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/gl_image_gl_texture.h"

#include <vector>

#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_R16 FOURCC('R', '1', '6', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_GR1616 FOURCC('G', 'R', '3', '2')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR2101010 FOURCC('A', 'B', '3', '0')
#define DRM_FORMAT_ARGB2101010 FOURCC('A', 'R', '3', '0')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')
#define DRM_FORMAT_P010 FOURCC('P', '0', '1', '0')

namespace media {
namespace {

// Returns corresponding internalformat if supported, and GL_NONE otherwise.
unsigned GLInternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::P010:
      return GL_RGB_YCBCR_P010_CHROMIUM;
    default:
      break;
  }
  return gl::BufferFormatToGLInternalFormat(format);
}

gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format) {
  switch (format) {
    case DRM_FORMAT_R8:
      return gfx::BufferFormat::R_8;
    case DRM_FORMAT_GR88:
      return gfx::BufferFormat::RG_88;
    case DRM_FORMAT_ABGR8888:
      return gfx::BufferFormat::RGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return gfx::BufferFormat::RGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return gfx::BufferFormat::BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return gfx::BufferFormat::BGRX_8888;
    case DRM_FORMAT_ABGR2101010:
      return gfx::BufferFormat::RGBA_1010102;
    case DRM_FORMAT_ARGB2101010:
      return gfx::BufferFormat::BGRA_1010102;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case DRM_FORMAT_YVU420:
      return gfx::BufferFormat::YVU_420;
    case DRM_FORMAT_P010:
      return gfx::BufferFormat::P010;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

}  // namespace

scoped_refptr<GLImageGLTexture> GLImageGLTexture::CreateFromTexture(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t texture_id) {
  auto image = base::WrapRefCounted(new GLImageGLTexture(size, format));
  if (!image->InitializeFromTexture(texture_id)) {
    return nullptr;
  }
  return image;
}

GLImageGLTexture::GLImageGLTexture(const gfx::Size& size,
                                   gfx::BufferFormat format)
    : egl_image_(EGL_NO_IMAGE_KHR),
      size_(size),
      format_(format),
      has_image_dma_buf_export_(gl::GLSurfaceEGL::GetGLDisplayEGL()
                                    ->ext->b_EGL_MESA_image_dma_buf_export) {}

GLImageGLTexture::~GLImageGLTexture() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    return;
  }

  const EGLBoolean result = eglDestroyImageKHR(
      gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), egl_image_);
  if (result == EGL_FALSE) {
    DLOG(ERROR) << "Error destroying EGLImage: " << ui::GetLastEGLErrorString();
  }
}

bool GLImageGLTexture::InitializeFromTexture(uint32_t texture_id) {
  if (GLInternalFormat(format_) == GL_NONE) {
    LOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format_);
    return false;
  }
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  if (!current_context || !current_context->IsCurrent(nullptr)) {
    LOG(ERROR) << "No gl context bound to the current thread";
    return false;
  }

  EGLContext context_handle =
      reinterpret_cast<EGLContext>(current_context->GetHandle());
  DCHECK_NE(context_handle, EGL_NO_CONTEXT);

  egl_image_ =
      eglCreateImageKHR(gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                        context_handle, EGL_GL_TEXTURE_2D_KHR,
                        reinterpret_cast<EGLClientBuffer>(texture_id), nullptr);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::NativePixmapHandle GLImageGLTexture::ExportHandle() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Must use Initialize.
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "GLImageGLTexture is not initialized";
    return gfx::NativePixmapHandle();
  }

  if (!has_image_dma_buf_export_) {
    LOG(ERROR) << "Error no extension EGL_MESA_image_dma_buf_export";
    return gfx::NativePixmapHandle();
  }

  int fourcc = 0;
  int num_planes = 0;
  EGLuint64KHR modifiers = 0;

  if (!eglExportDMABUFImageQueryMESA(
          gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), egl_image_,
          &fourcc, &num_planes, &modifiers)) {
    LOG(ERROR) << "Error querying EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  if (num_planes <= 0 || num_planes > 4) {
    LOG(ERROR) << "Invalid number of planes: " << num_planes;
    return gfx::NativePixmapHandle();
  }

  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc);
  if (format != format_) {
    // A driver has returned a format different than what has been requested.
    // This can happen if RGBX is implemented using RGBA. Otherwise there is
    // a real mistake from the user and we have to fail.
    if (GetInternalFormat() == GL_RGB &&
        format != gfx::BufferFormat::RGBA_8888) {
      LOG(ERROR) << "Invalid driver format: "
                 << gfx::BufferFormatToString(format)
                 << " for requested format: "
                 << gfx::BufferFormatToString(format_);
      return gfx::NativePixmapHandle();
    }
  }

  std::vector<int> fds(num_planes);
  std::vector<EGLint> strides(num_planes);
  std::vector<EGLint> offsets(num_planes);

  // It is specified for eglExportDMABUFImageMESA that the app is responsible
  // for closing any fds retrieved.
  if (!eglExportDMABUFImageMESA(
          gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), egl_image_,
          &fds[0], &strides[0], &offsets[0])) {
    LOG(ERROR) << "Error exporting EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  gfx::NativePixmapHandle handle{};
  handle.modifier = modifiers;
  for (int i = 0; i < num_planes; ++i) {
    // Sanity check. In principle all the fds are meant to be valid when
    // eglExportDMABUFImageMESA succeeds.
    base::ScopedFD scoped_fd(fds[i]);
    if (!scoped_fd.is_valid()) {
      LOG(ERROR) << "Invalid dmabuf";
      return gfx::NativePixmapHandle();
    }

    handle.planes.emplace_back(strides[i], offsets[i], 0 /* size opaque */,
                               std::move(scoped_fd));
  }

  return handle;
}

gfx::Size GLImageGLTexture::GetSize() {
  return size_;
}

void GLImageGLTexture::BindTexImage(unsigned target) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  glEGLImageTargetTexture2DOES(target, egl_image_);
}

unsigned GLImageGLTexture::GetInternalFormat() {
  return GLInternalFormat(format_);
}

}  // namespace media
