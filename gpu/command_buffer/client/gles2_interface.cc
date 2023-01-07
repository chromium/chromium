// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface.h"

#include <GLES2/gl2.h>

namespace gpu {
namespace gles2 {

GLboolean GLES2Interface::DidGpuSwitch(gl::GpuPreference* active_gpu) {
  return GL_FALSE;
}

}  // namespace gles2
}  // namespace gpu
