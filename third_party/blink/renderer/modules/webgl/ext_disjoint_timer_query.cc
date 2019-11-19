// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_disjoint_timer_query.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_timer_query_ext.h"

namespace blink {

WebGLExtensionName EXTDisjointTimerQuery::GetName() const {
  return kEXTDisjointTimerQueryName;
}

EXTDisjointTimerQuery* EXTDisjointTimerQuery::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<EXTDisjointTimerQuery>(context);
}

bool EXTDisjointTimerQuery::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_disjoint_timer_query");
}

const char* EXTDisjointTimerQuery::ExtensionName() {
  return "EXT_disjoint_timer_query";
}

WebGLTimerQueryEXT* EXTDisjointTimerQuery::createQueryEXT() {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return nullptr;

  return WebGLTimerQueryEXT::Create(scoped.Context());
}

void EXTDisjointTimerQuery::deleteQueryEXT(WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (!query || scoped.IsLost())
    return;
  query->DeleteObject(scoped.Context()->ContextGL());

  if (query == current_elapsed_query_)
    current_elapsed_query_.Clear();
}

GLboolean EXTDisjointTimerQuery::isQueryEXT(WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (!query || scoped.IsLost() || query->MarkedForDeletion() ||
      !query->Validate(nullptr, scoped.Context())) {
    return false;
  }

  return scoped.Context()->ContextGL()->IsQueryEXT(query->Object());
}

void EXTDisjointTimerQuery::beginQueryEXT(GLenum target,
                                          WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  if (!scoped.Context()->ValidateWebGLObject("beginQueryEXT", query))
    return;

  if (target != GL_TIME_ELAPSED_EXT) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "beginQueryEXT",
                                        "invalid target");
    return;
  }

  if (current_elapsed_query_) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION, "beginQueryEXT",
                                        "a query is already active for target");
    return;
  }

  if (query->HasTarget() && query->Target() != target) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION, "beginQueryEXT",
                                        "target does not match query");
    return;
  }

  scoped.Context()->ContextGL()->BeginQueryEXT(target, query->Object());
  query->SetTarget(target);
  current_elapsed_query_ = query;
}

void EXTDisjointTimerQuery::endQueryEXT(GLenum target) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  if (target != GL_TIME_ELAPSED_EXT) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "endQueryEXT",
                                        "invalid target");
    return;
  }

  if (!current_elapsed_query_) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION, "endQueryEXT",
                                        "no current query");
    return;
  }

  scoped.Context()->ContextGL()->EndQueryEXT(target);
  current_elapsed_query_->ResetCachedResult();
  current_elapsed_query_.Clear();
}

void EXTDisjointTimerQuery::queryCounterEXT(WebGLTimerQueryEXT* query,
                                            GLenum target) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;

  if (!scoped.Context()->ValidateWebGLObject("queryCounterEXT", query))
    return;

  if (target != GL_TIMESTAMP_EXT) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "queryCounterEXT",
                                        "invalid target");
    return;
  }

  if (query->HasTarget() && query->Target() != target) {
    scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION, "queryCounterEXT",
                                        "target does not match query");
    return;
  }

  // Timestamps are disabled in WebGL due to lack of driver support on multiple
  // platforms, so we don't actually perform a GL call.
  query->SetTarget(target);
  query->ResetCachedResult();
}

ScriptValue EXTDisjointTimerQuery::getQueryEXT(ScriptState* script_state,
                                               GLenum target,
                                               GLenum pname) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  if (pname == GL_QUERY_COUNTER_BITS_EXT) {
    if (target == GL_TIMESTAMP_EXT || target == GL_TIME_ELAPSED_EXT) {
      GLint value = 0;
      scoped.Context()->ContextGL()->GetQueryivEXT(target, pname, &value);
      return WebGLAny(script_state, value);
    }
    scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "getQuery",
                                        "invalid target/pname combination");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if (target == GL_TIME_ELAPSED_EXT && pname == GL_CURRENT_QUERY) {
    return current_elapsed_query_
               ? WebGLAny(script_state, current_elapsed_query_)
               : ScriptValue::CreateNull(script_state->GetIsolate());
  }

  if (target == GL_TIMESTAMP_EXT && pname == GL_CURRENT_QUERY) {
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "getQuery",
                                      "invalid target/pname combination");
  return ScriptValue::CreateNull(script_state->GetIsolate());
}

ScriptValue EXTDisjointTimerQuery::getQueryObjectEXT(ScriptState* script_state,
                                                     WebGLTimerQueryEXT* query,
                                                     GLenum pname) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  if (!scoped.Context()->ValidateWebGLObject("getQueryObjectEXT", query))
    return ScriptValue::CreateNull(script_state->GetIsolate());

  if (current_elapsed_query_ == query) {
    scoped.Context()->SynthesizeGLError(
        GL_INVALID_OPERATION, "getQueryObjectEXT", "query is currently active");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  switch (pname) {
    case GL_QUERY_RESULT_EXT: {
      query->UpdateCachedResult(scoped.Context()->ContextGL());
      return WebGLAny(script_state, query->GetQueryResult());
    }
    case GL_QUERY_RESULT_AVAILABLE_EXT: {
      query->UpdateCachedResult(scoped.Context()->ContextGL());
      return WebGLAny(script_state, query->IsQueryResultAvailable());
    }
    default:
      scoped.Context()->SynthesizeGLError(GL_INVALID_ENUM, "getQueryObjectEXT",
                                          "invalid pname");
      break;
  }

  return ScriptValue::CreateNull(script_state->GetIsolate());
}

void EXTDisjointTimerQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(current_elapsed_query_);
  WebGLExtension::Trace(visitor);
}

EXTDisjointTimerQuery::EXTDisjointTimerQuery(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_disjoint_timer_query");
}

}  // namespace blink
