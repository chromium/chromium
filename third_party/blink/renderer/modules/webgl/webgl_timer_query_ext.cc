// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_timer_query_ext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLTimerQueryEXT::WebGLTimerQueryEXT(WebGLRenderingContextBase* ctx)
    : WebGLContextObject(ctx),
      target_(0),
      query_id_(0),
      can_update_availability_(false),
      query_result_available_(false),
      query_result_(0),
      task_runner_(ctx->GetContextTaskRunner()) {
  Context()->ContextGL()->GenQueriesEXT(1, &query_id_);
}

WebGLTimerQueryEXT::~WebGLTimerQueryEXT() = default;

void WebGLTimerQueryEXT::ResetCachedResult() {
  can_update_availability_ = false;
  query_result_available_ = false;
  query_result_ = 0;
  // When this is called, the implication is that we should start
  // keeping track of whether we can update the cached availability
  // and result.
  ScheduleAllowAvailabilityUpdate();
}

void WebGLTimerQueryEXT::UpdateCachedResult(gpu::gles2::GLES2Interface* gl) {
  if (query_result_available_)
    return;

  if (!can_update_availability_)
    return;

  if (!HasTarget())
    return;

  // We can only update the cached result when control returns to the browser.
  can_update_availability_ = false;
  GLuint available = 0;
  gl->GetQueryObjectuivEXT(Object(), GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  query_result_available_ = !!available;
  if (query_result_available_) {
    GLuint64 result = 0;
    gl->GetQueryObjectui64vEXT(Object(), GL_QUERY_RESULT_EXT, &result);
    query_result_ = result;
    task_handle_.Cancel();
  } else {
    ScheduleAllowAvailabilityUpdate();
  }
}

bool WebGLTimerQueryEXT::IsQueryResultAvailable() {
  return query_result_available_;
}

GLuint64 WebGLTimerQueryEXT::GetQueryResult() {
  return query_result_;
}

void WebGLTimerQueryEXT::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteQueriesEXT(1, &query_id_);
  query_id_ = 0;
}

void WebGLTimerQueryEXT::ScheduleAllowAvailabilityUpdate() {
  if (task_handle_.IsActive())
    return;
  task_handle_ = PostCancellableTask(
      *task_runner_, FROM_HERE,
      WTF::BindOnce(&WebGLTimerQueryEXT::AllowAvailabilityUpdate,
                    WrapWeakPersistent(this)));
}

void WebGLTimerQueryEXT::AllowAvailabilityUpdate() {
  can_update_availability_ = true;
}

}  // namespace blink
