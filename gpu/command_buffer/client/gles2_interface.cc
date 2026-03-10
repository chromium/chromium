// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface.h"

#include <GLES2/gl2.h>

#include "components/viz/common/resources/shared_image_format.h"

namespace gpu {
namespace gles2 {

// static
bool GLES2Interface::CanCopySharedImageToGLTextureViaTextureCopy(
    const viz::SharedImageFormat& si_format,
    uint32_t texture_target) {
  const bool si_format_has_single_texture =
      si_format.is_single_plane() || si_format.PrefersExternalSampler();
  const bool si_usable_by_gles2_interface = texture_target != 0;

  // Copying the shared image to the destination texture via a direct
  // texture-to-texture copy requires being able to obtain a client-side GL
  // texture for the shared image, which in turn requires that the shared image
  // be either single-plane or use external sampler and that it be usable by GL.
  return si_format_has_single_texture && si_usable_by_gles2_interface;
}

GLboolean GLES2Interface::DidGpuSwitch(gl::GpuPreference* active_gpu) {
  return GL_FALSE;
}

}  // namespace gles2
}  // namespace gpu
