// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_query.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

WebGLQuery* WebGLQuery::Create(WebGL2RenderingContextBase* ctx) {
  return MakeGarbageCollected<WebGLQuery>(ctx);
}

WebGLQuery::WebGLQuery(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx),
      target_(0),
      can_update_availability_(false),
      query_result_available_(false),
      query_result_(0) {
  if (ctx->canvas()) {
    task_runner_ =
        ctx->canvas()->GetDocument().GetTaskRunner(TaskType::kInternalDefault);
  } else {
    // Fallback for OffscreenCanvas (no frame scheduler)
    task_runner_ = Thread::Current()->GetTaskRunner();
  }
  GLuint query;
  ctx->ContextGL()->GenQueriesEXT(1, &query);
  SetObject(query);
}

WebGLQuery::~WebGLQuery() = default;

void WebGLQuery::SetTarget(GLenum target) {
  DCHECK(Object());
  DCHECK(!target_);
  target_ = target;
}

void WebGLQuery::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteQueriesEXT(1, &object_);
  object_ = 0;
}

void WebGLQuery::ResetCachedResult() {
  can_update_availability_ = false;
  query_result_available_ = false;
  query_result_ = 0;
  // When this is called, the implication is that we should start
  // keeping track of whether we can update the cached availability
  // and result.
  ScheduleAllowAvailabilityUpdate();
}

void WebGLQuery::UpdateCachedResult(gpu::gles2::GLES2Interface* gl) {
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
    GLuint result = 0;
    gl->GetQueryObjectuivEXT(Object(), GL_QUERY_RESULT_EXT, &result);
    query_result_ = result;
    task_handle_.Cancel();
  } else {
    ScheduleAllowAvailabilityUpdate();
  }
}

bool WebGLQuery::IsQueryResultAvailable() {
  return query_result_available_;
}

GLuint WebGLQuery::GetQueryResult() {
  return query_result_;
}

void WebGLQuery::ScheduleAllowAvailabilityUpdate() {
  if (task_handle_.IsActive())
    return;
  task_handle_ =
      PostCancellableTask(*task_runner_, FROM_HERE,
                          WTF::Bind(&WebGLQuery::AllowAvailabilityUpdate,
                                    WrapWeakPersistent(this)));
}

void WebGLQuery::AllowAvailabilityUpdate() {
  can_update_availability_ = true;
}

}  // namespace blink
