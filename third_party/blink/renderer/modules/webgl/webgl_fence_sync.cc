// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_fence_sync.h"

#include <GLES2/gl2extchromium.h>
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

WebGLFenceSync::WebGLFenceSync(WebGL2RenderingContextBase* ctx,
                               GLenum condition,
                               GLbitfield flags)
    : WebGLSync(ctx, insertQuery(ctx), GL_SYNC_FENCE) {
  DCHECK(condition == GL_SYNC_GPU_COMMANDS_COMPLETE);
  DCHECK_EQ(flags, 0u);
}

GLuint WebGLFenceSync::insertQuery(WebGL2RenderingContextBase* ctx) {
  auto* gl = ctx->ContextGL();
  GLuint query = 0;
  gl->GenQueriesEXT(1, &query);
  gl->BeginQueryEXT(GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM, query);
  // This query is used like a fence. There doesn't need to be anything inside.
  gl->EndQueryEXT(GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM);
  return query;
}

}  // namespace blink
