// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/gl_image_egl_pixmap.h"

namespace media {

GLImageEGLPixmap::GLImageEGLPixmap(const gfx::Size& size)
    : binding_helper_(size) {}

GLImageEGLPixmap::~GLImageEGLPixmap() = default;

bool GLImageEGLPixmap::Initialize(x11::Pixmap pixmap) {
  return binding_helper_.Initialize(pixmap);
}

gfx::Size GLImageEGLPixmap::GetSize() {
  return binding_helper_.GetSize();
}

bool GLImageEGLPixmap::BindTexImage(unsigned target) {
  return binding_helper_.BindTexImage(target);
}

void GLImageEGLPixmap::ReleaseEGLImage() {
  return binding_helper_.ReleaseEGLImage();
}

}  // namespace media
