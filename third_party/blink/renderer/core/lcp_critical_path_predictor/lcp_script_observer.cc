// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_script_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

LCPScriptObserver::~LCPScriptObserver() = default;

HashSet<String> LCPScriptObserver::GetExecutingScriptUrls() {
  HashSet<String> script_urls;

  // Gather sync and async scripts in execution
  for (const probe::ExecuteScript* probe : stack_script_probes_) {
    if (probe->script_url.empty()) {
      continue;
    }
    script_urls.insert(probe->script_url);
  }

  // Gather async functions in execution
  for (const probe::CallFunction* probe : stack_function_probes_) {
    String url = GetScriptUrlFromCallFunctionProbe(probe);
    if (url.empty()) {
      continue;
    }
    script_urls.insert(url);
  }

  // Gather (promise) microtasks in execution. This is required as Probes
  // do not yet have an implementation that covers microtasks.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  DCHECK(isolate);
  auto v8_stack_urls = GetScriptUrlsFromCurrentStack(isolate, 0);
  for (auto& url : v8_stack_urls) {
    if (url.empty()) {
      continue;
    }
    script_urls.insert(url);
  }

  const String document_url = local_root_->GetDocument()->Url();
  if (!document_url.empty()) {
    script_urls.erase(document_url);
  }

  return script_urls;
}

String LCPScriptObserver::GetScriptUrlFromCallFunctionProbe(
    const probe::CallFunction* probe) {
  v8::Local<v8::Value> resource_name =
      probe->function->GetScriptOrigin().ResourceName();
  String script_url;
  if (!resource_name.IsEmpty()) {
    v8::MaybeLocal<v8::String> resource_name_string =
        resource_name->ToString(ToIsolate(local_root_)->GetCurrentContext());
    if (!resource_name_string.IsEmpty()) {
      script_url = ToCoreString(resource_name_string.ToLocalChecked());
    }
  }
  return script_url;
}

LCPScriptObserver::LCPScriptObserver(LocalFrame* local_root)
    : local_root_(local_root) {
  CHECK(base::FeatureList::IsEnabled(features::kLCPScriptObserver));
  local_root_->GetProbeSink()->AddLCPScriptObserver(this);
}

void LCPScriptObserver::Will(const probe::ExecuteScript& probe) {
  stack_script_probes_.push_back(&probe);
}

void LCPScriptObserver::Did(const probe::ExecuteScript& probe) {
  DCHECK(!stack_script_probes_.empty());
  stack_script_probes_.pop_back();
}

void LCPScriptObserver::Will(const probe::CallFunction& probe) {
  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (probe.depth) {
    return;
  }
  stack_function_probes_.push_back(&probe);
}

void LCPScriptObserver::Did(const probe::CallFunction& probe) {
  if (probe.depth) {
    return;
  }
  DCHECK(!stack_function_probes_.empty());
  stack_function_probes_.pop_back();
}

void LCPScriptObserver::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
}

}  // namespace blink
