// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/native_image_buffer.h"

#include <list>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

#if !BUILDFLAG(IS_MAC)
class NativeImageBufferEGL : public NativeImageBuffer {
 public:
  static scoped_refptr<NativeImageBufferEGL> Create(GLuint texture_id);

  NativeImageBufferEGL(const NativeImageBufferEGL&) = delete;
  NativeImageBufferEGL& operator=(const NativeImageBufferEGL&) = delete;

 private:
  NativeImageBufferEGL(EGLDisplay display, EGLImageKHR image);
  ~NativeImageBufferEGL() override;
  void AddClient(gl::GLImage* client) override;
  void RemoveClient(gl::GLImage* client) override;
  bool IsClient(gl::GLImage* client) override;
  void BindToTexture(GLenum target) const override;

  const EGLDisplay egl_display_;
  const EGLImageKHR egl_image_;

  base::Lock lock_;

  struct ClientInfo {
    explicit ClientInfo(gl::GLImage* client);
    ~ClientInfo();

    raw_ptr<gl::GLImage> client;
    bool needs_wait_before_read;
  };
  std::list<ClientInfo> client_infos_;
  raw_ptr<gl::GLImage> write_client_;
};

scoped_refptr<NativeImageBufferEGL> NativeImageBufferEGL::Create(
    GLuint texture_id) {
  gl::GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  EGLDisplay egl_display = display->GetDisplay();
  EGLContext egl_context = eglGetCurrentContext();

  DCHECK_NE(EGL_NO_CONTEXT, egl_context);
  DCHECK_NE(EGL_NO_DISPLAY, egl_display);
  DCHECK(glIsTexture(texture_id));

  DCHECK(display->ext->b_EGL_KHR_image_base &&
         display->ext->b_EGL_KHR_gl_texture_2D_image &&
         gl::g_current_gl_driver->ext.b_GL_OES_EGL_image);

  const EGLint egl_attrib_list[] = {
      EGL_GL_TEXTURE_LEVEL_KHR, 0, EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  EGLClientBuffer egl_buffer = reinterpret_cast<EGLClientBuffer>(texture_id);
  EGLenum egl_target = EGL_GL_TEXTURE_2D_KHR;

  EGLImageKHR egl_image = eglCreateImageKHR(
      egl_display, egl_context, egl_target, egl_buffer, egl_attrib_list);

  if (egl_image == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "eglCreateImageKHR for cross-thread sharing failed: 0x"
               << std::hex << eglGetError();
    return nullptr;
  }

  return new NativeImageBufferEGL(egl_display, egl_image);
}

NativeImageBufferEGL::ClientInfo::ClientInfo(gl::GLImage* client)
    : client(client), needs_wait_before_read(true) {}

NativeImageBufferEGL::ClientInfo::~ClientInfo() = default;

NativeImageBufferEGL::NativeImageBufferEGL(EGLDisplay display,
                                           EGLImageKHR image)
    : egl_display_(display),
      egl_image_(image),
      write_client_(nullptr) {
  DCHECK(egl_display_ != EGL_NO_DISPLAY);
  DCHECK(egl_image_ != EGL_NO_IMAGE_KHR);
}

NativeImageBufferEGL::~NativeImageBufferEGL() {
  DCHECK(client_infos_.empty());
  if (egl_image_ != EGL_NO_IMAGE_KHR)
    eglDestroyImageKHR(egl_display_, egl_image_);
}

void NativeImageBufferEGL::AddClient(gl::GLImage* client) {
  base::AutoLock lock(lock_);
  client_infos_.emplace_back(client);
}

void NativeImageBufferEGL::RemoveClient(gl::GLImage* client) {
  base::AutoLock lock(lock_);
  if (write_client_ == client)
    write_client_ = nullptr;
  for (std::list<ClientInfo>::iterator it = client_infos_.begin();
       it != client_infos_.end(); it++) {
    if (it->client == client) {
      client_infos_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

bool NativeImageBufferEGL::IsClient(gl::GLImage* client) {
  base::AutoLock lock(lock_);
  for (auto & client_info : client_infos_) {
    if (client_info.client == client)
      return true;
  }
  return false;
}

void NativeImageBufferEGL::BindToTexture(GLenum target) const {
  DCHECK(egl_image_ != EGL_NO_IMAGE_KHR);
  glEGLImageTargetTexture2DOES(target, egl_image_);
  DCHECK_EQ(static_cast<EGLint>(EGL_SUCCESS), eglGetError());
  DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

#endif

class NativeImageBufferStub : public NativeImageBuffer {
 public:
  NativeImageBufferStub() = default;

  NativeImageBufferStub(const NativeImageBufferStub&) = delete;
  NativeImageBufferStub& operator=(const NativeImageBufferStub&) = delete;

 private:
  ~NativeImageBufferStub() override = default;
  void AddClient(gl::GLImage* client) override {}
  void RemoveClient(gl::GLImage* client) override {}
  bool IsClient(gl::GLImage* client) override { return true; }
  void BindToTexture(GLenum target) const override {}
};

}  // anonymous namespace

// static
scoped_refptr<NativeImageBuffer> NativeImageBuffer::Create(GLuint texture_id) {
  switch (gl::GetGLImplementation()) {
#if !BUILDFLAG(IS_MAC)
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return NativeImageBufferEGL::Create(texture_id);
#endif
    case gl::kGLImplementationMockGL:
    case gl::kGLImplementationStubGL:
      return new NativeImageBufferStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace gles2
}  // namespace gpu
