// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_sync.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_object_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebGLSync::WebGLSync(WebGLContextObjectSupport* ctx,
                     GLuint object,
                     GLenum object_type)
    : WebGLObject(ctx),
      sync_status_(GL_UNSIGNALED),
      object_type_(object_type),
      task_runner_(ctx->GetContextTaskRunner()) {
  SetObject(object);
  ScheduleAllowCacheUpdate();
}

WebGLSync::~WebGLSync() = default;

void WebGLSync::UpdateCache(gpu::gles2::GLES2Interface* gl) {
  // Context loss is checked at higher levels.

  if (sync_status_ == GL_SIGNALED) {
    return;
  }

  if (!allow_cache_update_) {
    return;
  }

  // We can only update the cached result when control returns to the browser.
  allow_cache_update_ = false;
  GLuint value = 0;
  gl->GetQueryObjectuivEXT(Object(), GL_QUERY_RESULT_AVAILABLE, &value);
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
}

bool WebGLSync::IsSignaled() const {
  return (sync_status_ == GL_SIGNALED);
}

void WebGLSync::ScheduleAllowCacheUpdate() {
  if (task_handle_.IsActive())
    return;
  task_handle_ = PostCancellableTask(
      *task_runner_, FROM_HERE,
      BindOnce(&WebGLSync::AllowCacheUpdate, WrapWeakPersistent(this)));
}

void WebGLSync::AllowCacheUpdate() {
  allow_cache_update_ = true;
}

void WebGLSync::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteQueriesEXT(1, &Object());
}

}  // namespace blink
