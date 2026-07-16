// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"

#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

// static
ScriptInitiationMonitor* ScriptInitiationMonitor::FromExecutionContext(
    ExecutionContext* execution_context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (LocalFrame* frame = window->GetFrame()) {
      return frame->GetScriptInitiationMonitor();
    }
  }
  return nullptr;
}

ScriptInitiationMonitor::ScriptInitiationMonitor(LocalFrame* local_root)
    : local_root_(local_root) {
  local_root_->GetProbeSink()->AddScriptInitiationMonitor(this);
}

ScriptInitiationMonitor::~ScriptInitiationMonitor() {
  DCHECK(!local_root_);
}

void ScriptInitiationMonitor::Shutdown() {
  if (!local_root_) {
    return;
  }
  local_root_->GetProbeSink()->RemoveScriptInitiationMonitor(this);
  local_root_ = nullptr;
}

void ScriptInitiationMonitor::AddObserver(Observer* observer) {
  observers_.insert(observer);
}

void ScriptInitiationMonitor::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

void ScriptInitiationMonitor::Will(const probe::ExecuteScript& probe) {
  if (!probe.context) {
    return;
  }
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  LazyStackTrace stack_trace(isolate);
  V8ScriptId script_id(probe.script_id);
  for (auto& observer : observers_) {
    if (observer) {
      observer->WillExecuteScript(*probe.context, probe.v8_context, script_id,
                                  probe.script_url, stack_trace);
    }
  }
}

void ScriptInitiationMonitor::Did(const probe::ExecuteScript& probe) {
  if (!probe.context) {
    return;
  }
  V8ScriptId script_id(probe.script_id);
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidExecuteScript(script_id);
    }
  }
}

void ScriptInitiationMonitor::Will(const probe::CallFunction& probe) {
  if (!probe.context) {
    return;
  }
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  LazyStackTrace stack_trace(isolate);
  V8ScriptId script_id;
  if (isolate && !probe.function.IsEmpty()) {
    script_id = V8ScriptId(probe.function->ScriptId());
  }
  const bool is_nested = probe.depth > 0;
  for (auto& observer : observers_) {
    if (observer) {
      observer->WillCallFunction(*probe.context, script_id, is_nested,
                                 stack_trace);
    }
  }
}

void ScriptInitiationMonitor::Did(const probe::CallFunction& probe) {
  if (!probe.context) {
    return;
  }
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  V8ScriptId script_id;
  if (isolate && !probe.function.IsEmpty()) {
    script_id = V8ScriptId(probe.function->ScriptId());
  }
  const bool is_nested = probe.depth > 0;
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidCallFunction(script_id, is_nested);
    }
  }
}

void ScriptInitiationMonitor::DidCreateAsyncTask(
    probe::AsyncTaskContext* task_context) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  LazyStackTrace stack_trace(isolate);
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidCreateAsyncTask(task_context, stack_trace);
    }
  }
}

void ScriptInitiationMonitor::DidStartAsyncTask(
    probe::AsyncTaskContext* task_context) {
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidStartAsyncTask(task_context);
    }
  }
}

void ScriptInitiationMonitor::DidFinishAsyncTask(
    probe::AsyncTaskContext* task_context) {
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidFinishAsyncTask(task_context);
    }
  }
}

void ScriptInitiationMonitor::PrepareRequest(
    DocumentLoader* loader,
    const ResourceRequestHead& request,
    std::optional<KURL> alias_url,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info,
    std::optional<AdProvenance> known_ad_provenance,
    bool scan_javascript_stack) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  LazyStackTrace stack_trace(isolate);
  Document* document = loader && loader->GetFrame()
                           ? loader->GetFrame()->GetDocument()
                           : nullptr;
  for (auto& observer : observers_) {
    if (observer) {
      observer->WillPrepareRequest(document, request, alias_url, resource_type,
                                   initiator_info, known_ad_provenance,
                                   scan_javascript_stack, stack_trace);
    }
  }
}

void ScriptInitiationMonitor::DidRegisterDynamicScript(
    v8::Local<v8::Context> v8_context,
    V8ScriptId script_id) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  LazyStackTrace stack_trace(isolate);
  for (auto& observer : observers_) {
    if (observer) {
      observer->DidRegisterDynamicScript(v8_context, script_id, stack_trace);
    }
  }
}

void ScriptInitiationMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(observers_);
}

}  // namespace blink
