// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_capabilities.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "build/build_config.h"

namespace gpu {

#if BUILDFLAG(IS_MAC)
SharedImageCapabilities::SharedImageCapabilities()
    :  // Initialize `macos_specific_texture_target` to a value that is valid
       // for ClientSharedImage to use, as unittests broadly create and use
       // SharedImageCapabilities instances without initializing this field. The
       // specific value is chosen to match the historical default value that
       // was used when this state was accessed via a global variable.
       // TODO(crbug.com/41494843): Fix all unittest flows to set this field and
       // then initialize this field to 0, requiring that clients initialize it
       // to the proper value (as is always done in production).
       // NOTE: We do this here rather than in the header file due to needing to
       // include the GLES2 headers, which causes compile issues when put in the
       // header files.
      macos_specific_texture_target(GL_TEXTURE_RECTANGLE_ARB) {}
#else
SharedImageCapabilities::SharedImageCapabilities() = default;
#endif
SharedImageCapabilities::SharedImageCapabilities(
    const SharedImageCapabilities& other) = default;
SharedImageCapabilities::~SharedImageCapabilities() = default;

}  // namespace gpu
