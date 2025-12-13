// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Maps a MonkeyPatchableApi enum value to the corresponding property path
// to access that API, starting from the context's global object.
std::vector<const char*> GetApiPropertyPath(AdTracker::MonkeyPatchableApi api) {
  switch (api) {
    case AdTracker::MonkeyPatchableApi::kHistoryPushState:
      return {"history", "pushState"};
    case AdTracker::MonkeyPatchableApi::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

// A struct to hold the results from `GetApiFunctionInfo`.
struct ApiFunctionInfo {
  v8::MaybeLocal<v8::Function> function;

  // True if the API appears to be monkey patched. False if the API appears to
  // be the native implementation or if an error occurred during the check.
  bool is_monkey_patched = false;
};

// Finds the V8 function for a given API, checks if it has been monkey patched,
// and returns both pieces of information.
ApiFunctionInfo GetApiFunctionInfo(v8::Isolate* isolate,
                                   AdTracker::MonkeyPatchableApi api) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty()) {
    return {};
  }

  v8::Context::Scope context_scope(context);

  // Start with the global object.
  v8::Local<v8::Value> current_value = context->Global();
  const std::vector<const char*> property_path = GetApiPropertyPath(api);

  // Traverse the property path (e.g., global object -> `history` ->
  // `pushState`).
  for (const char* property_name : property_path) {
    // Each intermediate value in the path must be an object.
    if (!current_value->IsObject()) {
      return {};
    }

    v8::Local<v8::Object> current_object = current_value.As<v8::Object>();
    v8::Local<v8::String> property_key =
        v8::String::NewFromUtf8(isolate, property_name).ToLocalChecked();

    v8::MaybeLocal<v8::Value> maybe_next_value =
        current_object->Get(context, property_key);

    // If the property doesn't exist, the chain is broken.
    if (maybe_next_value.IsEmpty()) {
      return {};
    }
    current_value = maybe_next_value.ToLocalChecked();
  }

  // At the end of the path, we expect a function. If it's not a function,
  // it has been tampered with, and we can't perform our check.
  if (!current_value->IsFunction()) {
    return {};
  }

  v8::Local<v8::Function> api_function = current_value.As<v8::Function>();

  // Native functions will have an invalid script ID. User-defined functions
  // (monkey patches) will have a valid one.
  bool is_monkey_patched =
      api_function->ScriptId() != v8::Message::kNoScriptIdInfo;

  return {handle_scope.Escape(api_function), is_monkey_patched};
}

bool IsKnownAdExecutionContext(ExecutionContext* execution_context) {
  // TODO(jkarlin): Do the same check for worker contexts.
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    LocalFrame* frame = window->GetFrame();
    if (frame && frame->IsAdFrame())
      return true;
  }
  return false;
}

String GenerateFakeUrlFromScriptId(int script_id) {
  // Null string is used to represent scripts with neither a name nor an ID.
  if (script_id == v8::Message::kNoScriptIdInfo)
    return String();

  // The prefix cannot appear in real URLs.
  return String::Format("{ id %d }", script_id);
}

v8_inspector::V8DebuggerId GetDebuggerIdForContext(
    const v8::Local<v8::Context>& v8_context) {
  if (v8_context.IsEmpty()) {
    return v8_inspector::V8DebuggerId();
  }
  int contextId = v8_inspector::V8ContextInfo::executionContextId(v8_context);
  ThreadDebugger* thread_debugger =
      ThreadDebugger::From(v8::Isolate::GetCurrent());
  DCHECK(thread_debugger);
  v8_inspector::V8Inspector* inspector = thread_debugger->GetV8Inspector();
  DCHECK(inspector);
  return inspector->uniqueDebuggerId(contextId);
}

}  // namespace

String AdTracker::AdScriptAncestry::ToString() const {
  if (ancestry_chain.empty() || !root_script_filterlist_rule.IsValid()) {
    return "";
  }

  StringBuilder builder;
  builder.AppendFormat("Debug info: adscript '%s' ",
                       ancestry_chain[0].name.Ascii().c_str());
  for (size_t i = 1; i < ancestry_chain.size(); ++i) {
    builder.AppendFormat("(loaded by '%s') ",
                         ancestry_chain[i].name.Ascii().c_str());
  }
  builder.AppendFormat("matched ad filterlist rule: %s",
                       root_script_filterlist_rule.ToString().c_str());
  return builder.ReleaseString();
}

// static
AdTracker* AdTracker::FromExecutionContext(
    ExecutionContext* execution_context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (LocalFrame* frame = window->GetFrame()) {
      return frame->GetAdTracker();
    }
  }
  return nullptr;
}

// static
bool AdTracker::IsAdScriptExecutingInDocument(Document* document,
                                              StackType stack_type) {
  AdTracker* ad_tracker =
      document->GetFrame() ? document->GetFrame()->GetAdTracker() : nullptr;
  return ad_tracker && ad_tracker->IsAdScriptInStack(stack_type);
}

AdTracker::AdTracker(LocalFrame* local_root) : local_root_(local_root) {
  local_root_->GetProbeSink()->AddAdTracker(this);
}

AdTracker::~AdTracker() {
  DCHECK(!local_root_);
}

void AdTracker::Shutdown() {
  if (!local_root_)
    return;
  local_root_->GetProbeSink()->RemoveAdTracker(this);
  local_root_ = nullptr;
}

ExecutionContext* AdTracker::GetCurrentExecutionContext(v8::Isolate* isolate) {
  if (!isolate) {
    return nullptr;
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return context.IsEmpty() ? nullptr : ToExecutionContext(context);
}

void AdTracker::Will(const probe::ExecuteScript& probe) {
  running_sync_tasks_++;

  if (probe.script_id <= 0) {
    return;
  }

  // We're executing a script's top-level. This is our first time seeing the
  // script id for the given url.
  bool is_inline_script = probe.script_url.empty();

  String url = is_inline_script ? GenerateFakeUrlFromScriptId(probe.script_id)
                                : probe.script_url;

  bool is_ad = IsKnownAdScript(probe.context, url);

  // For inline scripts, this is our opportunity to check the stack to see if
  // an ad created it. Scripts that are loaded asynchronously will create
  // probe::AsyncTasks.
  std::optional<AdScriptIdentifier> ancestor_ad_script;
  if (!is_ad && is_inline_script &&
      IsAdScriptInStackHelper(StackType::kBottomAndTop,
                              /*ignore_monkey_patch=*/MonkeyPatchableApi::kNone,
                              &ancestor_ad_script)) {
    AdProvenance ad_provenance;
    if (ancestor_ad_script.has_value()) {
      ad_provenance = ancestor_ad_script->id;
    } else {
      // This can happen if the script originates from an ad context without
      // further traceable script (crbug.com/421202278).
      ad_provenance = NoProvenance{};
    }
    AppendToKnownAdScripts(*probe.context, url, std::move(ad_provenance));
    is_ad = true;
  }

  // Since this is our first time running the script, this is the first we've
  // seen of its script id. Record the id so that we can refer to the script
  // by id rather than string.
  if (is_ad && !IsKnownAdExecutionContext(probe.context)) {
    OnScriptIdAvailableForKnownAdScript(probe.context, probe.v8_context, url,
                                        probe.script_id);
  }

  if (is_ad && !bottom_most_ad_script_.has_value()) {
    bottom_most_ad_script_ = probe.script_id;
  }
}

void AdTracker::Did(const probe::ExecuteScript& probe) {
  running_sync_tasks_--;
  if (running_sync_tasks_ == 0) {
    ad_monkey_patch_calls_in_scope_.clear();
  }

  if (bottom_most_ad_script_.has_value() &&
      bottom_most_ad_script_.value() == probe.script_id) {
    bottom_most_ad_script_.reset();
  }
}

void AdTracker::Will(const probe::CallFunction& probe) {
  running_sync_tasks_++;

  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (probe.depth || probe.function->ScriptId() <= 0) {
    return;
  }

  if (!bottom_most_ad_script_.has_value() &&
      ad_script_data_.Contains(probe.function->ScriptId())) {
    bottom_most_ad_script_ = probe.function->ScriptId();
  }
}

void AdTracker::Did(const probe::CallFunction& probe) {
  running_sync_tasks_--;
  if (running_sync_tasks_ == 0) {
    ad_monkey_patch_calls_in_scope_.clear();
  }

  if (probe.depth) {
    return;
  }
  if (bottom_most_ad_script_.has_value() &&
      bottom_most_ad_script_.value() == probe.function->ScriptId()) {
    bottom_most_ad_script_.reset();
  }
}

bool AdTracker::CalculateIfAdSubresource(
    ExecutionContext* execution_context,
    const KURL& request_url,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info,
    bool known_ad,
    bool scan_stack_for_ads,
    const subresource_filter::ScopedRule& rule) {
  DCHECK(!rule.IsValid() || known_ad);

  // Check if the document loading the resource is an ad.
  const bool is_ad_execution_context =
      IsKnownAdExecutionContext(execution_context);
  known_ad = known_ad || is_ad_execution_context;

  // We skip script checking for stylesheet-initiated resource requests as the
  // stack may represent the cause of a style recalculation rather than the
  // actual resources themselves. Instead, the ad bit is set according to the
  // CSSParserContext when the request is made. See crbug.com/1051605.
  if (initiator_info.name == fetch_initiator_type_names::kCSS ||
      initiator_info.name == fetch_initiator_type_names::kUacss) {
    return known_ad;
  }

  // Check if any executing script is an ad.
  std::optional<AdScriptIdentifier> ancestor_ad_script;
  if (scan_stack_for_ads) {
    known_ad =
        known_ad || IsAdScriptInStackHelper(
                        StackType::kBottomAndTop,
                        /*ignore_monkey_patch=*/MonkeyPatchableApi::kNone,
                        &ancestor_ad_script);
  }

  // If it is a script marked as an ad and it's not in an ad context, append it
  // to the known ad script set. We don't need to keep track of ad scripts in ad
  // contexts, because any script executed inside an ad context is considered an
  // ad script by IsKnownAdScript.
  if (resource_type == ResourceType::kScript && known_ad &&
      !is_ad_execution_context) {
    DCHECK(!ancestor_ad_script || !rule.IsValid());

    AdProvenance ad_provenance;
    if (!ancestor_ad_script && !rule.IsValid()) {
      ad_provenance = NoProvenance{};
    } else if (ancestor_ad_script) {
      ad_provenance = ancestor_ad_script->id;
    } else {
      DCHECK(rule.IsValid());
      ad_provenance = rule;
    }
    AppendToKnownAdScripts(*execution_context, request_url.GetString(),
                           std::move(ad_provenance));
  }

  return known_ad;
}

void AdTracker::DidCreateAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  std::optional<AdScriptIdentifier> id;
  if (IsAdScriptInStackHelper(StackType::kBottomAndTop,
                              /*ignore_monkey_patch=*/MonkeyPatchableApi::kNone,
                              &id)) {
    task_context->SetAdTask(id);
  }
}

void AdTracker::DidStartAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  CHECK(task_context->IsAdTask());
  if (running_ad_async_tasks_ == 0) {
    DCHECK(!bottom_most_async_ad_script_.has_value());
    bottom_most_async_ad_script_ = task_context->ad_identifier();
  }

  running_ad_async_tasks_ += 1;
}

void AdTracker::DidFinishAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  CHECK(task_context->IsAdTask());
  DCHECK_GE(running_ad_async_tasks_, 1);
  running_ad_async_tasks_ -= 1;
  if (running_ad_async_tasks_ == 0) {
    bottom_most_async_ad_script_.reset();
  }
}

bool AdTracker::IsAdScriptInStack(StackType stack_type,
                                  MonkeyPatchableApi ignore_monkey_patch,
                                  AdScriptAncestry* out_ad_script_ancestry) {
  std::optional<AdScriptIdentifier> out_ad_script;

  std::optional<AdScriptIdentifier>* out_ad_script_ptr =
      out_ad_script_ancestry ? &out_ad_script : nullptr;

  bool is_ad_script_in_stack = IsAdScriptInStackHelper(
      stack_type, ignore_monkey_patch, out_ad_script_ptr);

  if (out_ad_script.has_value()) {
    CHECK(out_ad_script_ancestry);
    CHECK(is_ad_script_in_stack);
    *out_ad_script_ancestry = GetAncestry(out_ad_script.value());
  }

  return is_ad_script_in_stack;
}

bool AdTracker::IsAdScriptInStackHelper(
    StackType stack_type,
    MonkeyPatchableApi ignore_monkey_patch,
    std::optional<AdScriptIdentifier>* out_ad_script) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  ExecutionContext* execution_context = GetCurrentExecutionContext(isolate);
  if (!execution_context)
    return false;

  // If we're in an ad context, then no matter what the executing script is it's
  // considered an ad. To enhance traceability, we attempt to return the
  // identifier of the ad script that created the targeted ad frame. Note that
  // this may still return `nullopt`; refer to `LocalFrame::CreationAdScript`
  // for details.
  if (IsKnownAdExecutionContext(execution_context)) {
    if (out_ad_script) {
      if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
        if (LocalFrame* frame = window->GetFrame()) {
          *out_ad_script = frame->CreationAdScript();
        }
      }
    }
    return true;
  }

  // We check this after checking for an ad context because we don't keep track
  // of script ids for ad frames.
  if (bottom_most_ad_script_.has_value()) {
    if (out_ad_script) {
      auto it = ad_script_data_.find(bottom_most_ad_script_.value());
      if (it != ad_script_data_.end()) {
        *out_ad_script = it->value.id;
      }
    }
    return true;
  }

  // We check if async is on stack after sync, because sync is likely easier to
  // reason about.
  if (running_ad_async_tasks_ > 0) {
    if (out_ad_script) {
      *out_ad_script = bottom_most_async_ad_script_;
    }
    return true;
  }

  if (stack_type == StackType::kBottomOnly)
    return false;

  // If we're not aware of any ad scripts at all, or any scripts in this
  // context, don't bother looking at the stack.
  if (ad_script_data_.empty()) {
    return false;
  }
  if (auto it = context_known_ad_scripts_.find(execution_context);
      it == context_known_ad_scripts_.end() || it->value.empty()) {
    return false;
  }

  // The stack scanned by the AdTracker contains entry points into the stack
  // (e.g., when v8 is executed) but not the entire stack. For a small cost we
  // can also check the top of the stack (this is much cheaper than getting the
  // full stack from v8).
  int top_script_id = v8::StackTrace::CurrentScriptId(isolate);
  if (top_script_id <= 0) {
    return false;
  }

  auto script_it = ad_script_data_.find(top_script_id);
  if (script_it == ad_script_data_.end()) {
    return false;
  }

  // Heuristic to avoid misattributing calls due to monkey patching. If the top
  // script is an ad script but not the bottom, then determine if the API call
  // was initiated by ad script or not. If it wasn't initiated by ad script,
  // then let it through once.
  if (ignore_monkey_patch != MonkeyPatchableApi::kNone &&
      IsFirstCallOfApiFromNonAdScript(isolate, ignore_monkey_patch)) {
    return false;
  }

  if (out_ad_script) {
    *out_ad_script = script_it->value.id;
  }

  return true;
}

bool AdTracker::IsFirstCallOfApiFromNonAdScript(v8::Isolate* isolate,
                                                MonkeyPatchableApi api) {
  // This heuristic is only applied when `running_sync_tasks_ > 0`. This is
  // because its state (`ad_monkey_patch_calls_in_scope_`) is scoped to a
  // synchronous task, relying on the `Will`/`Did` probe pairs for setup and
  // teardown. Promise callbacks do not trigger these probes, so applying the
  // heuristic there would lead to incorrect state.
  if (running_sync_tasks_ <= 0) {
    return false;
  }

  // The heuristic only applies on the first call to an API within a task.
  if (base::Contains(ad_monkey_patch_calls_in_scope_, api)) {
    return false;
  }

  if (WasApiCalledByNonAdScript(isolate, api)) {
    ad_monkey_patch_calls_in_scope_.insert(api);
    return true;
  }

  return false;
}

bool AdTracker::WasApiCalledByNonAdScript(v8::Isolate* isolate,
                                          MonkeyPatchableApi api) const {
  ApiFunctionInfo api_info = GetApiFunctionInfo(isolate, api);
  if (!api_info.is_monkey_patched) {
    return false;
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Function> api_function;
  if (!api_info.function.ToLocal(&api_function)) {
    return false;
  }

  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/10);

  // The expected monkey patch pattern requires a non-ad script calling an ad
  // script. Thus, the stack must have at least two frames.
  if (stack_trace.IsEmpty() || stack_trace->GetFrameCount() <= 1) {
    return false;
  }

  // To distinguish the expected monkey patch pattern from an ad-driven
  // "just-in-time" patch, we walk the stack to find the boundary between ad and
  // non-ad script frames.
  for (int i = 1; i < stack_trace->GetFrameCount(); ++i) {
    v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, i);
    if (frame.IsEmpty()) {
      return false;
    }

    // This frame is still from an ad script, so continue up the stack.
    if (ad_script_data_.Contains(frame->GetScriptId())) {
      continue;
    }

    // Frame `i` is the first non-ad script. The previous frame (`i-1`) must be
    // the ad script entry point. We expect this to be the patched API itself.
    v8::Local<v8::StackFrame> ad_barrier_frame =
        stack_trace->GetFrame(isolate, i - 1);
    if (ad_barrier_frame.IsEmpty()) {
      return false;
    }

    // Verify the function at the boundary is the patched API by checking its
    // script ID and name.
    if (ad_barrier_frame->GetScriptId() != api_function->ScriptId()) {
      return false;
    }

    v8::Local<v8::String> barrier_func_name =
        ad_barrier_frame->GetFunctionName();
    v8::Local<v8::Value> api_func_name_value = api_function->GetDebugName();

    if (!barrier_func_name.IsEmpty() && !barrier_func_name->IsUndefined() &&
        api_func_name_value->IsString()) {
      v8::Local<v8::String> api_func_name =
          api_func_name_value.As<v8::String>();

      v8::Local<v8::Context> context = isolate->GetCurrentContext();
      if (barrier_func_name->Equals(context, api_func_name).FromMaybe(false)) {
        return true;
      }
    }

    // If the function names don't match, it doesn't fit the expected pattern
    // (e.g., a "just-in-time" patch).
    return false;
  }

  // If the loop completes, the entire stack trace is from ad scripts, so the
  // call did not originate from a non-ad script.
  return false;
}

bool AdTracker::IsKnownAdScript(ExecutionContext* execution_context,
                                const String& url) {
  if (!execution_context)
    return false;

  if (IsKnownAdExecutionContext(execution_context))
    return true;

  if (url.empty()) {
    return false;
  }

  auto it = context_known_ad_scripts_.find(execution_context);
  if (it == context_known_ad_scripts_.end()) {
    return false;
  }
  return it->value.Contains(url);
}

// This is a separate function for testing purposes.
void AdTracker::AppendToKnownAdScripts(ExecutionContext& execution_context,
                                       const String& url,
                                       AdProvenance ad_provenance) {
  DCHECK(!url.empty());

  auto add_result = context_known_ad_scripts_.insert(
      &execution_context, KnownAdScriptsAndProvenance());

  KnownAdScriptsAndProvenance& known_ad_scripts_and_provenance =
      add_result.stored_value->value;

  // While technically the same script URL can be loaded with different
  // provenances (e.g., from different ancestors), we track only the first
  // association for simplicity.
  known_ad_scripts_and_provenance.insert(url, std::move(ad_provenance));
}

void AdTracker::OnScriptIdAvailableForKnownAdScript(
    ExecutionContext* execution_context,
    const v8::Local<v8::Context>& v8_context,
    const String& script_name,
    int script_id) {
  DCHECK(!script_name.empty());
  DCHECK_NE(v8::Message::kNoScriptIdInfo, script_id);
  auto it = context_known_ad_scripts_.find(execution_context);
  DCHECK(it != context_known_ad_scripts_.end());

  const KnownAdScriptsAndProvenance& known_ad_scripts_and_provenance =
      it->value;

  auto known_ad_script_it = known_ad_scripts_and_provenance.find(script_name);
  DCHECK(known_ad_script_it != known_ad_scripts_and_provenance.end());

  const AdProvenance& ad_provenance = known_ad_script_it->value;

  // Note that multiple script executions might originate from the same script
  // URL, and are intended to share the same provenance. While this approach
  // might not perfectly mirror the script loading ancestry in all complex
  // scenarios, it's considered sufficient for our tracking purposes.
  ad_script_data_.insert(
      script_id,
      AdScriptData(AdScriptIdentifier(GetDebuggerIdForContext(v8_context),
                                      script_id, script_name),
                   ad_provenance));
}

AdTracker::AdScriptAncestry AdTracker::GetAncestry(
    const AdScriptIdentifier& ad_script) {
  AdTracker::AdScriptAncestry ancestry;

  // TODO(yaoxia): Determine if we should CHECK that that the script ID in each
  // step is guaranteed to be present in `ad_script_data_`.
  auto provenance_it = ad_script_data_.find(ad_script.id);
  if (provenance_it == ad_script_data_.end()) {
    return ancestry;
  }

  HashSet<int> seen_script_ids;
  bool duplicate = false;

  ancestry.ancestry_chain.push_back(provenance_it->value.id);
  seen_script_ids.insert(provenance_it->value.id.id);

  while (provenance_it != ad_script_data_.end()) {
    const AdProvenance& ad_provenance = provenance_it->value.provenance;

    // Update `ancestry` based on the type of the `ad_provenance` variant.
    bool root_reached = std::visit(
        absl::Overload{[&](NoProvenance) { return true; },
                       [&](int script_id) {
                         // Prevent an infinite loop due to cycles.
                         if (!seen_script_ids.insert(script_id).is_new_entry) {
                           duplicate = true;
                           return true;
                         }

                         auto it = this->ad_script_data_.find(script_id);
                         ancestry.ancestry_chain.push_back(it->value.id);

                         // Move on to the next ancestor.
                         return false;
                       },
                       [&](const subresource_filter::ScopedRule& rule) {
                         ancestry.root_script_filterlist_rule = rule;
                         // We've reached the ruleset rule which is our
                         // "root", so stop.
                         return true;
                       }},
        ad_provenance);

    if (root_reached) {
      break;
    }

    provenance_it = ad_script_data_.find(ancestry.ancestry_chain.back().id);
  }

  base::UmaHistogramBoolean(
      "Navigation.IframeCreated.AdTracker.DuplicateAncestryScriptId",
      duplicate);

  return ancestry;
}

void AdTracker::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(context_known_ad_scripts_);
}

}  // namespace blink
