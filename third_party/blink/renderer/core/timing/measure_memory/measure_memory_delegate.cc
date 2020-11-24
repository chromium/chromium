// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_attribution.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_breakdown_entry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_memory_measurement.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

MeasureMemoryDelegate::MeasureMemoryDelegate(v8::Isolate* isolate,
                                             v8::Local<v8::Context> context,
                                             ResultCallback callback)
    : isolate_(isolate),
      context_(isolate, context),
      callback_(std::move(callback)) {
  context_.SetPhantom();
}

// Returns true if the given context should be included in the current memory
// measurement. Currently it is very conservative and allows only the same
// origin contexts that belong to the same JavaScript origin.
// With COOP/COEP we will be able to relax this restriction for the contexts
// that opt-in into memory measurement.
bool MeasureMemoryDelegate::ShouldMeasure(v8::Local<v8::Context> context) {
  if (context_.IsEmpty()) {
    // The original context was garbage collected in the meantime.
    return false;
  }
  v8::Local<v8::Context> original_context = context_.NewLocal(isolate_);
  ExecutionContext* original_execution_context =
      ExecutionContext::From(original_context);
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!original_execution_context || !execution_context) {
    // One of the contexts is detached or is created by DevTools.
    return false;
  }
  if (original_execution_context->GetAgent() != execution_context->GetAgent()) {
    // Context do not belong to the same JavaScript agent.
    return false;
  }
  if (ScriptState::From(context)->World().IsIsolatedWorld()) {
    // Context belongs to an extension. Skip it.
    return false;
  }
  const SecurityOrigin* original_security_origin =
      original_execution_context->GetSecurityContext().GetSecurityOrigin();
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityContext().GetSecurityOrigin();
  if (!original_security_origin->IsSameOriginWith(security_origin)) {
    // TODO(ulan): Allow only same-origin contexts until the implementation
    // is switched to PerformanceManager.
    return false;
  }
  return true;
}

namespace {
// Helper functions for constructing a memory measurement result.

LocalFrame* GetLocalFrame(v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (!window) {
    // The context was detached. Ignore it.
    return nullptr;
  }
  return window->GetFrame();
}

String GetUrl(const LocalFrame* requesting_frame, const LocalFrame* frame) {
  // Only same-origin frames are reported until we switch to PerformanceManager.
  DCHECK(requesting_frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
      frame->GetSecurityContext()->GetSecurityOrigin()));
  return frame->GetDocument()->Url().GetString();
}

MemoryAttribution* CreateMemoryAttribution(const String& url) {
  auto* result = MemoryAttribution::Create();
  result->setUrl(url);
  result->setScope("Window");
  result->setContainer(nullptr);
  return result;
}

MemoryBreakdownEntry* CreateMemoryBreakdownEntry(size_t bytes,
                                                 const Vector<String>& types,
                                                 const String& url) {
  auto* result = MemoryBreakdownEntry::Create();
  result->setBytes(bytes);
  result->setUserAgentSpecificTypes(types);
  HeapVector<Member<MemoryAttribution>> attribution;
  if (url.length()) {
    attribution.push_back(CreateMemoryAttribution(url));
  }
  result->setAttribution(attribution);
  return result;
}

}  // anonymous namespace

// Constructs a memory measurement result based on the given list of (context,
// size) pairs and resolves the promise.
void MeasureMemoryDelegate::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t unattributed_size) {
  if (context_.IsEmpty()) {
    // The context was garbage collected in the meantime.
    return;
  }
  v8::Local<v8::Context> context = context_.NewLocal(isolate_);
  LocalFrame* requesting_frame = GetLocalFrame(context);
  if (!requesting_frame) {
    // The context was detached in the meantime.
    return;
  }
  v8::Context::Scope context_scope(context);
  size_t total_size = 0;
  for (const auto& context_size : context_sizes) {
    total_size += context_size.second;
  }
  HeapVector<Member<MemoryBreakdownEntry>> breakdown;
  size_t detached_size = 0;
  const String kJS("JS");
  const Vector<String> js_types = {kJS};
  for (const auto& it : context_sizes) {
    size_t bytes = it.second;
    LocalFrame* frame = GetLocalFrame(it.first);
    if (!frame) {
      detached_size += bytes;
      continue;
    }
    String url = GetUrl(requesting_frame, frame);
    breakdown.push_back(CreateMemoryBreakdownEntry(bytes, js_types, url));
  }
  if (detached_size) {
    const String kDetached("Detached");
    breakdown.push_back(CreateMemoryBreakdownEntry(
        detached_size, Vector<String>{kJS, kDetached}, ""));
  }
  if (unattributed_size) {
    breakdown.push_back(
        CreateMemoryBreakdownEntry(unattributed_size, Vector<String>{kJS}, ""));
  }
  std::move(callback_).Run(breakdown);
}

}  // namespace blink
