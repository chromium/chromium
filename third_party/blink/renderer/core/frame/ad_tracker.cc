// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

bool IsKnownAdExecutionContext(ExecutionContext* execution_context) {
  // TODO(jkarlin): Do the same check for worker contexts.
  if (auto* document = DynamicTo<Document>(execution_context)) {
    LocalFrame* frame = document->GetFrame();
    if (frame && frame->IsAdSubframe())
      return true;
  }
  return false;
}

}  // namespace

AdTracker::AdTracker(LocalFrame* local_root) : local_root_(local_root) {
  local_root_->GetProbeSink()->addAdTracker(this);
}

AdTracker::~AdTracker() {
  DCHECK(!local_root_);
}

void AdTracker::Shutdown() {
  if (!local_root_)
    return;
  local_root_->GetProbeSink()->removeAdTracker(this);
  local_root_ = nullptr;
}

String AdTracker::ScriptAtTopOfStack(ExecutionContext* execution_context) {
  std::unique_ptr<blink::SourceLocation> current_stack_trace =
      SourceLocation::Capture(execution_context);
  // TODO(jkarlin): Url() sometimes returns String(), why?
  return current_stack_trace ? current_stack_trace->Url() : "";
}

ExecutionContext* AdTracker::GetCurrentExecutionContext() {
  // Determine the current ExecutionContext.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return context.IsEmpty() ? nullptr : ToExecutionContext(context);
}

void AdTracker::WillExecuteScript(ExecutionContext* execution_context,
                                  const String& script_url) {
  bool is_ad = script_url.IsEmpty()
                   ? false
                   : IsKnownAdScript(execution_context, script_url);
  stack_frame_is_ad_.push_back(is_ad);
  if (is_ad)
    num_ads_in_stack_ += 1;
}

void AdTracker::DidExecuteScript() {
  if (stack_frame_is_ad_.back()) {
    DCHECK_LT(0u, num_ads_in_stack_);
    num_ads_in_stack_ -= 1;
  }
  stack_frame_is_ad_.pop_back();
}

void AdTracker::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.context, probe.script_url);
}

void AdTracker::Did(const probe::ExecuteScript& probe) {
  DidExecuteScript();
}

void AdTracker::Will(const probe::CallFunction& probe) {
  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (probe.depth)
    return;

  v8::Local<v8::Value> resource_name =
      probe.function->GetScriptOrigin().ResourceName();
  String script_url;
  if (!resource_name.IsEmpty())
    script_url = ToCoreString(resource_name->ToString(ToIsolate(local_root_)));
  WillExecuteScript(probe.context, script_url);
}

void AdTracker::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;

  DidExecuteScript();
}

void AdTracker::WillSendRequest(ExecutionContext* execution_context,
                                unsigned long identifier,
                                DocumentLoader* loader,
                                ResourceRequest& request,
                                const ResourceResponse& redirect_response,
                                const FetchInitiatorInfo& initiator_info,
                                ResourceType resource_type) {
  // If the resource is not already marked as an ad, check if the document
  // loading the resource is an ad or if any executing script is an ad.
  if (!request.IsAdResource() &&
      (IsKnownAdExecutionContext(execution_context) || IsAdScriptInStack())) {
    request.SetIsAdResource();
  }

  // If it is a script marked as an ad and it's not in an ad context, append it
  // to the known ad script set. We don't need to keep track of ad scripts in ad
  // contexts, because any script executed inside an ad context is considered an
  // ad script by IsKnownAdScript.
  if (resource_type == ResourceType::kScript && request.IsAdResource() &&
      !IsKnownAdExecutionContext(execution_context)) {
    AppendToKnownAdScripts(*execution_context, request.Url().GetString());
  }
}

bool AdTracker::IsAdScriptInStack() {
  if (num_ads_in_stack_ > 0)
    return true;

  ExecutionContext* execution_context = GetCurrentExecutionContext();
  if (!execution_context)
    return false;

  // If we're in an ad context, then no matter what the executing script is it's
  // considered an ad.
  if (IsKnownAdExecutionContext(execution_context))
    return true;

  // The pseudo-stack contains entry points into the stack (e.g., when v8 is
  // executed) but not the entire stack. It's cheap to retrieve the top of the
  // stack so scan that as well.
  String top_script = ScriptAtTopOfStack(execution_context);

  if (!top_script.IsEmpty() && IsKnownAdScript(execution_context, top_script))
    return true;

  return false;
}

bool AdTracker::IsKnownAdScript(ExecutionContext* execution_context,
                                const String& url) {
  if (!execution_context)
    return false;

  if (IsKnownAdExecutionContext(execution_context))
    return true;

  auto it = known_ad_scripts_.find(execution_context);
  if (it == known_ad_scripts_.end())
    return false;
  return it->value.find(url) != it->value.end();
}

// This is a separate function for testing purposes.
void AdTracker::AppendToKnownAdScripts(ExecutionContext& execution_context,
                                       const String& url) {
  auto add_result =
      known_ad_scripts_.insert(&execution_context, HashSet<String>());
  add_result.stored_value->value.insert(url);
}

void AdTracker::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_root_);
  visitor->Trace(known_ad_scripts_);
}

}  // namespace blink
