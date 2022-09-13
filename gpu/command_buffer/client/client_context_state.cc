// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_context_state.h"

#include "base/check_op.h"

namespace gpu {
namespace gles2 {

ClientContextState::ClientContextState() = default;

ClientContextState::~ClientContextState() = default;

void ClientContextState::SetViewport(
    GLint x, GLint y, GLsizei width, GLsizei height) {
  DCHECK_LE(0, width);
  DCHECK_LE(0, height);

  viewport_x = x;
  viewport_y = y;
  viewport_width = width;
  viewport_height = height;
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/client_context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu
