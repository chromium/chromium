// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"

namespace blink {

void GLES2InterfaceForTests::GenTextures(GLsizei n, GLuint* textures) {
  static GLuint id = 1;
  for (GLsizei i = 0; i < n; ++i) {
    textures[i] = id++;
  }
}

// ImplementationBase implementation
void GLES2InterfaceForTests::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  static gpu::CommandBufferId::Generator command_buffer_id_generator;
  gpu::SyncToken source(gpu::GPU_IO,
                        command_buffer_id_generator.GenerateNextId(), 2);
  memcpy(sync_token, &source, sizeof(source));
}

}  // namespace blink
