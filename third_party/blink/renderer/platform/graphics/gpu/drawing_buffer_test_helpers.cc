// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"

#include "base/compiler_specific.h"

namespace blink {

void GLES2InterfaceForTests::GenTextures(GLsizei n, GLuint* textures) {
  static GLuint id = 1;
  for (GLsizei i = 0; i < n; ++i) {
    UNSAFE_TODO(textures[i]) = id++;
  }
}

void GLES2InterfaceForTests::GenRenderbuffers(GLsizei n,
                                              GLuint* renderbuffers) {
  static GLuint id = 1;
  // SAFETY: This mock implements the legacy OpenGL GLES2 API which uses raw
  // pointers to pass arrays. The size of the buffer is guaranteed by the caller
  // to be at least 'n'.
  UNSAFE_BUFFERS({
    for (GLsizei i = 0; i < n; ++i) {
      renderbuffers[i] = id++;
    }
  });
}

void GLES2InterfaceForTests::GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  static GLuint id = 1;
  // SAFETY: This mock implements the legacy OpenGL GLES2 API which uses raw
  // pointers to pass arrays. The size of the buffer is guaranteed by the caller
  // to be at least 'n'.
  UNSAFE_BUFFERS({
    for (GLsizei i = 0; i < n; ++i) {
      framebuffers[i] = id++;
    }
  });
}

// ImplementationBase implementation
void GLES2InterfaceForTests::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  static gpu::CommandBufferId::Generator command_buffer_id_generator;
  gpu::SyncToken source(gpu::GPU_IO,
                        command_buffer_id_generator.GenerateNextId(), 2);
  UNSAFE_TODO(memcpy(sync_token, &source, sizeof(source)));
}

}  // namespace blink
