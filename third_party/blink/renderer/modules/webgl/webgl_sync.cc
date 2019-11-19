// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_sync.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

WebGLSync::WebGLSync(WebGL2RenderingContextBase* ctx,
                     GLuint object,
                     GLenum object_type)
    : WebGLSharedObject(ctx),
      sync_status_(GL_UNSIGNALED),
      object_(object),
      object_type_(object_type) {
  if (ctx->canvas()) {
    task_runner_ =
        ctx->canvas()->GetDocument().GetTaskRunner(TaskType::kInternalDefault);
  } else {
    // Fallback for OffscreenCanvas (no frame scheduler)
    task_runner_ = Thread::Current()->GetTaskRunner();
  }
  ScheduleAllowCacheUpdate();
}

WebGLSync::~WebGLSync() = default;

void WebGLSync::UpdateCache(gpu::gles2::GLES2Interface* gl) {
  if (sync_status_ == GL_SIGNALED) {
    return;
  }

  if (!allow_cache_update_) {
    return;
  }

  // We can only update the cached result when control returns to the browser.
  allow_cache_update_ = false;
  GLuint value = 0;
  gl->GetQueryObjectuivEXT(object_, GL_QUERY_RESULT_AVAILABLE, &value);
  if (value == GL_TRUE) {
    sync_status_ = GL_SIGNALED;
  } else {
    sync_status_ = GL_UNSIGNALED;
    ScheduleAllowCacheUpdate();
  }
}

GLint WebGLSync::GetCachedResult(GLenum pname) {
  switch (pname) {
    case GL_OBJECT_TYPE:
      return object_type_;
    case GL_SYNC_STATUS:
      return sync_status_;
    case GL_SYNC_CONDITION:
      return GL_SYNC_GPU_COMMANDS_COMPLETE;
    case GL_SYNC_FLAGS:
      return 0;
  }

  NOTREACHED();
  return 0;
}

bool WebGLSync::IsSignaled() const {
  return (sync_status_ == GL_SIGNALED);
}

void WebGLSync::ScheduleAllowCacheUpdate() {
  if (task_handle_.IsActive())
    return;
  task_handle_ = PostCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::Bind(&WebGLSync::AllowCacheUpdate, WrapWeakPersistent(this)));
}

void WebGLSync::AllowCacheUpdate() {
  allow_cache_update_ = true;
}

void WebGLSync::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteQueriesEXT(1, &object_);
  object_ = 0;
}

}  // namespace blink
