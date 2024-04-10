/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

const Node& OpaqueRootForGC(v8::Isolate*, const Node* node) {
  DCHECK(node);
  if (node->isConnected())
    return node->GetDocument();

  if (auto* attr = DynamicTo<Attr>(node)) {
    Node* owner_element = attr->ownerElement();
    if (!owner_element)
      return *node;
    node = owner_element;
  }

  while (Node* parent = node->ParentOrShadowHostOrTemplateHostNode())
    node = parent;

  return *node;
}

}  // namespace

// static
v8::EmbedderGraph::Node::Detachedness V8GCController::DetachednessFromWrapper(
    v8::Isolate* isolate,
    const v8::Local<v8::Value>& v8_value,
    uint16_t,
    void*) {
  const WrapperTypeInfo* wrapper_type_info =
      ToWrapperTypeInfo(v8_value.As<v8::Object>());
  if (wrapper_type_info->wrapper_class_id != WrapperTypeInfo::kNodeClassId) {
    return v8::EmbedderGraph::Node::Detachedness::kUnknown;
  }
  const auto& root_node = OpaqueRootForGC(
      isolate, V8Node::ToWrappableUnsafe(isolate, v8_value.As<v8::Object>()));
  if (root_node.isConnected() && root_node.GetExecutionContext()) {
    return v8::EmbedderGraph::Node::Detachedness::kAttached;
  }
  return v8::EmbedderGraph::Node::Detachedness::kDetached;
}

void V8GCController::GcPrologue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcPrologue);
  auto* per_isolate_data = V8PerIsolateData::From(isolate);
  per_isolate_data->EnterGC();

  ScriptForbiddenScope::Enter();

  v8::HandleScope scope(isolate);
  switch (type) {
    case v8::kGCTypeIncrementalMarking:
      // Recomputing ASWs is opportunistic during incremental marking as they
      // only need to be recomputing during the atomic pause for corectness.
      if (per_isolate_data->GetActiveScriptWrappableManager()) {
        per_isolate_data->GetActiveScriptWrappableManager()
            ->RecomputeActiveScriptWrappables(
                ActiveScriptWrappableManager::RecomputeMode::kOpportunistic);
      }
      break;
    case v8::kGCTypeMarkSweepCompact:
      if (per_isolate_data->GetActiveScriptWrappableManager()) {
        per_isolate_data->GetActiveScriptWrappableManager()
            ->RecomputeActiveScriptWrappables(
                ActiveScriptWrappableManager::RecomputeMode::kRequired);
      }
      break;
    default:
      break;
  }
}

void V8GCController::GcEpilogue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcEpilogue);

  V8PerIsolateData::From(isolate)->LeaveGC();

  ScriptForbiddenScope::Exit();

  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters",
      TRACE_EVENT_SCOPE_THREAD, "data", [&](perfetto::TracedValue context) {
        inspector_update_counters_event::Data(std::move(context), isolate);
      });
}

}  // namespace blink
