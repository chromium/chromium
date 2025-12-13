// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_timer_query_ext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_object_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebGLTimerQueryEXT::WebGLTimerQueryEXT(WebGLContextObjectSupport* ctx)
    : WebGLObject(ctx),
      target_(0),
      can_update_availability_(false),
      query_result_available_(false),
      query_result_(0),
      task_runner_(ctx->GetContextTaskRunner()) {
  if (!ctx || ctx->IsLost()) {
    return;
  }

  GLuint query = 0;
  ctx->ContextGL()->GenQueriesEXT(1, &query);
  SetObject(query);
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
  // Context loss is checked at higher levels.

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
  gl->DeleteQueriesEXT(1, &Object());
}

void WebGLTimerQueryEXT::ScheduleAllowAvailabilityUpdate() {
  if (task_handle_.IsActive())
    return;
  task_handle_ =
      PostCancellableTask(*task_runner_, FROM_HERE,
                          BindOnce(&WebGLTimerQueryEXT::AllowAvailabilityUpdate,
                                   WrapWeakPersistent(this)));
}

void WebGLTimerQueryEXT::AllowAvailabilityUpdate() {
  can_update_availability_ = true;
}

}  // namespace blink
