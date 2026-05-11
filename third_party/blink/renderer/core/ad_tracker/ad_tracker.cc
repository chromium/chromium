// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
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
base::span<const char* const> GetApiPropertyPath(
    AdTracker::MonkeyPatchableApi api) {
  switch (api) {
    case AdTracker::MonkeyPatchableApi::kHistoryPushState: {
      static const char* const kPath[] = {"history", "pushState"};
      return kPath;
    }
    case AdTracker::MonkeyPatchableApi::kHistoryReplaceState: {
      static const char* const kPath[] = {"history", "replaceState"};
      return kPath;
    }
    case AdTracker::MonkeyPatchableApi::kNodeAppendChild: {
      static const char* const kPath[] = {"Node", "prototype", "appendChild"};
      return kPath;
    }
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
  const base::span<const char* const> property_path = GetApiPropertyPath(api);

  // Traverse the property path (e.g., global object -> `history` ->
  // `pushState`).
  for (const char* property_name : property_path) {
    // Each intermediate value in the path must be an object.
    if (!current_value->IsObject()) {
      return {};
    }

    v8::Local<v8::Object> current_object = current_value.As<v8::Object>();
    v8::Local<v8::String> property_key = V8AtomicString(isolate, property_name);

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
    if (frame && frame->IsAdFrame()) {
      return true;
    }
  }
  return false;
}

String GenerateFakeUrlFromScriptId(V8ScriptId script_id) {
  // Null string is used to represent scripts with neither a name nor an ID.
  if (script_id == AdScriptIdentifier::kEmptyId) {
    return String();
  }

  // The prefix cannot appear in real URLs.
  return String::Format("{ id %d }", script_id.value());
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
    return String();
  }

  StringBuilder builder;
  builder.Append("Debug info: adscript '");
  builder.Append(ancestry_chain[0].name);
  builder.Append("' ");
  for (size_t i = 1; i < ancestry_chain.size(); ++i) {
    builder.Append("(loaded by '");
    builder.Append(ancestry_chain[i].name);
    builder.Append("') ");
  }
  builder.Append("matched ad filterlist rule: ");
  builder.Append(String::FromUtf8(root_script_filterlist_rule.ToString()));
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
  if (!local_root_) {
    return;
  }
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

  V8ScriptId script_id(probe.script_id);

  // We're executing a script's top-level. This is our first time seeing the
  // script id for the given url.
  bool is_inline_script = probe.script_url.empty();

  String url = is_inline_script ? GenerateFakeUrlFromScriptId(script_id)
                                : probe.script_url;

  bool is_ad = IsKnownAdScript(probe.context, url);

  // For inline scripts, this is our opportunity to check the stack to see if
  // an ad created it. Scripts that are loaded asynchronously will create
  // probe::AsyncTasks.
  std::optional<AdScriptIdentifier> ancestor_ad_script;
  if (!is_ad && is_inline_script &&
      IsAdScriptInStackHelper(StackType::kTopOnly,
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
                                        script_id);
  }

  if (is_ad && !bottom_most_ad_script_.has_value()) {
    bottom_most_ad_script_ = script_id;
  }
}

void AdTracker::Did(const probe::ExecuteScript& probe) {
  running_sync_tasks_--;
  if (running_sync_tasks_ == 0) {
    ad_monkey_patch_calls_in_scope_.clear();
  }

  if (bottom_most_ad_script_.has_value() &&
      bottom_most_ad_script_.value() == V8ScriptId(probe.script_id)) {
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

  V8ScriptId script_id(probe.function->ScriptId());
  if (!bottom_most_ad_script_.has_value() &&
      ad_script_data_.Contains(script_id)) {
    bottom_most_ad_script_ = script_id;
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
      bottom_most_ad_script_.value() ==
          V8ScriptId(probe.function->ScriptId())) {
    bottom_most_ad_script_.reset();
  }
}

std::optional<AdProvenance> AdTracker::CalculateIfAdSubresource(
    ExecutionContext* execution_context,
    const KURL& request_url,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info,
    std::optional<AdProvenance> known_ad_provenance,
    bool scan_stack_for_ads) {
  // Check if the document loading the resource is an ad.
  const bool is_ad_execution_context =
      IsKnownAdExecutionContext(execution_context);

  if (!known_ad_provenance && is_ad_execution_context) {
    known_ad_provenance = NoProvenance{};
  }

  // We skip script checking for stylesheet-initiated resource requests as the
  // stack may represent the cause of a style recalculation rather than the
  // actual resources themselves. Instead, the ad bit is set according to the
  // CSSParserContext when the request is made. See crbug.com/1051605.
  if (initiator_info.name == fetch_initiator_type_names::kCSS ||
      initiator_info.name == fetch_initiator_type_names::kUacss) {
    return known_ad_provenance;
  }

  // Check if any executing script is an ad.
  if (!known_ad_provenance && scan_stack_for_ads) {
    std::optional<AdScriptIdentifier> ancestor_ad_script;
    if (IsAdScriptInStackHelper(
            StackType::kTopOnly,
            /*ignore_monkey_patch=*/MonkeyPatchableApi::kNodeAppendChild,
            &ancestor_ad_script)) {
      known_ad_provenance = ancestor_ad_script
                                ? AdProvenance(ancestor_ad_script->id)
                                : AdProvenance(NoProvenance{});
    }
  }

  // If it is a script marked as an ad and it's not in an ad context, append it
  // to the known ad script set. We don't need to keep track of ad scripts in ad
  // contexts, because any script executed inside an ad context is considered an
  // ad script by IsKnownAdScript.
  if (resource_type == ResourceType::kScript && known_ad_provenance &&
      !is_ad_execution_context) {
    AppendToKnownAdScripts(*execution_context, request_url.GetString(),
                           *known_ad_provenance);
  }

  return known_ad_provenance;
}

void AdTracker::DidCreateAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  std::optional<AdScriptIdentifier> id;
  if (IsAdScriptInStackHelper(StackType::kTopOnly,
                              /*ignore_monkey_patch=*/MonkeyPatchableApi::kNone,
                              &id)) {
    task_context->SetAdTask(id);
  }
}

void AdTracker::DidStartAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  async_script_stack_.push_back(task_context->ad_identifier());
}

void AdTracker::DidFinishAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  async_script_stack_.pop_back();
}

void AdTracker::RegisterAdScript(
    v8::Local<v8::Context> v8_context,
    V8ScriptId script_id,
    const std::optional<AdScriptIdentifier>& parent_ad_script) {
  DCHECK_NE(v8::Message::kNoScriptIdInfo, script_id.value());
  String script_name = GenerateFakeUrlFromScriptId(script_id);

  AdProvenance provenance;
  if (parent_ad_script.has_value() &&
      parent_ad_script->id != AdScriptIdentifier::kEmptyId) {
    provenance = parent_ad_script->id;
  } else {
    provenance = NoProvenance{};
  }

  ad_script_data_.insert(
      script_id,
      AdScriptData(AdScriptIdentifier(GetDebuggerIdForContext(v8_context),
                                      script_id, script_name),
                   std::move(provenance)));
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
    *out_ad_script_ancestry = GetAncestry(out_ad_script.value().id);
  }

  return is_ad_script_in_stack;
}

bool AdTracker::IsAdScriptInStackHelper(
    StackType stack_type,
    MonkeyPatchableApi ignore_monkey_patch,
    std::optional<AdScriptIdentifier>* out_ad_script) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();

  // If we're in an ad context, then no matter what the executing script is it's
  // considered an ad. To enhance traceability, we attempt to return the
  // identifier of the ad script that created the targeted ad frame. Note that
  // this may still return `nullopt`; refer to `LocalFrame::CreationAdScript`
  // for details.
  if (ExecutionContext* execution_context = GetCurrentExecutionContext(isolate);
      execution_context && IsKnownAdExecutionContext(execution_context)) {
    if (out_ad_script) {
      if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
        if (LocalFrame* frame = window->GetFrame()) {
          *out_ad_script = frame->CreationAdScript();
        }
      }
    }
    return true;
  }

  if (stack_type == StackType::kBottomOnly) {
    // We check this after checking for an ad context because we don't keep
    // track of script ids for ad frames.
    if (bottom_most_ad_script_.has_value()) {
      if (out_ad_script) {
        auto it = ad_script_data_.find(bottom_most_ad_script_.value());
        if (it != ad_script_data_.end()) {
          *out_ad_script = it->value.id;
        }
      }
      return true;
    }

    // We check if async is on stack after sync, because sync is likely easier
    // to reason about.
    for (auto& script : async_script_stack_) {
      if (script.has_value()) {
        if (out_ad_script) {
          *out_ad_script = *script;
        }
        return true;
      }
    }
    return false;
  }

  // If we're not aware of any ad scripts at all don't bother looking at the
  // stack.
  if (ad_script_data_.empty()) {
    return false;
  }

  // When the `ignore_monkey_patch` heuristic is specified, we inspect the top
  // five stack frames instead of just the top frame. It allows us to capture
  // publisher monkey patch scenarios (i.e., one or more publisher monkey
  // patches that passively invoke an ad's intent).
  std::array<v8::StackTrace::ScriptData, 5> stack_buffer;
  size_t limit = (ignore_monkey_patch != MonkeyPatchableApi::kNone) ? 5 : 1;
  auto stack = v8::StackTrace::CurrentScriptData(
      isolate,
      v8::MemorySpan<v8::StackTrace::ScriptData>(stack_buffer.data(), limit));

  if (stack.empty()) {
    // There is nothing on the v8 stack. This means that we're in some
    // asynchronous continuation in blink code. Fall back on the async stack.
    if (!async_script_stack_.empty() &&
        async_script_stack_.back().has_value()) {
      if (out_ad_script) {
        *out_ad_script = async_script_stack_.back();
      }
      return true;
    }

    return false;
  }

  std::optional<AdScriptIdentifier> matched_ad_script;
  int ad_script_index = -1;

  for (size_t i = 0; i < stack.size(); ++i) {
    V8ScriptId script_id(stack[i].id);
    if (script_id.value() <= 0) {
      return false;
    }

    auto it = ad_script_data_.find(script_id);
    if (it != ad_script_data_.end()) {
      ad_script_index = static_cast<int>(i);
      matched_ad_script = it->value.id;
      break;
    }
  }

  if (!matched_ad_script.has_value()) {
    // The top scripts on the stack are not registered ad script. Are they
    // from ad frames?

    // If the top scripts on the stack are non-ad, then we consider the stack
    // to be non-ad related, as publisher script may be running an event
    // callback.
    return false;
  }

  if (ad_script_index > 0) {
    // The top script on the stack is non-ad, but a script further down (at
    // `ad_script_index`) is an ad. If an ad is calling a monkeypatched non-ad
    // API, consider it still ad related.
    if (ignore_monkey_patch != MonkeyPatchableApi::kNone &&
        IsFunctionAMonkeyPatch(isolate, stack[ad_script_index - 1].function,
                               ignore_monkey_patch)) {
      if (out_ad_script) {
        *out_ad_script = *matched_ad_script;
      }
      return true;
    }

    // Otherwise, consider the stack non-ad-related. This prevents false
    // positives where publisher script may be running an event callback.
    return false;
  }

  // The top of the stack is an ad script. This heuristic avoids misattributing
  // calls due to monkey patching. If the top script is an ad script but not the
  // bottom, then determine if the API call was initiated by ad script or not.
  // If it wasn't initiated by ad script, then let it through once.
  if (ignore_monkey_patch != MonkeyPatchableApi::kNone &&
      IsFirstCallOfApiFromNonAdScript(isolate, ignore_monkey_patch)) {
    return false;
  }

  if (out_ad_script) {
    *out_ad_script = *matched_ad_script;
  }

  return true;
}

bool AdTracker::IsFirstCallOfApiFromNonAdScript(v8::Isolate* isolate,
                                                MonkeyPatchableApi api) {
  // The heuristic only applies on the first call to an API within a task.
  // Note, running_sync_tasks_ will be 0 when in a promise callback microtask,
  // since we're not monitoring promises we don't apply the allow-once heuristic
  // in that scenario.
  if (running_sync_tasks_ > 0 &&
      ad_monkey_patch_calls_in_scope_.Contains(api)) {
    return false;
  }

  if (WasApiCalledByNonAdScript(isolate, api)) {
    if (running_sync_tasks_ > 0) {
      ad_monkey_patch_calls_in_scope_.insert(api);
    }
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

  std::array<v8::StackTrace::ScriptData, 10> stack_buffer;
  auto stack_trace = v8::StackTrace::CurrentScriptData(isolate, stack_buffer);

  // The expected monkey patch pattern requires a non-ad script calling an ad
  // script. Thus, the stack must have at least two frames.
  if (stack_trace.empty() || stack_trace.size() <= 1) {
    return false;
  }

  // To distinguish the expected monkey patch pattern from an ad-driven
  // "just-in-time" patch, we walk the stack to find the boundary between ad and
  // non-ad script frames.
  for (size_t i = 1; i < stack_trace.size(); ++i) {
    v8::StackTrace::ScriptData& frame = stack_trace[i];

    // If this frame is still ad related, continue up the stack.
    if (ad_script_data_.Contains(V8ScriptId(frame.id))) {
      continue;
    }

    // Frame `i` is the first non-ad script. The previous frame (`i-1`) must be
    // the ad script entry point. We expect this to be the patched API itself.
    v8::StackTrace::ScriptData& ad_barrier_frame = stack_trace[i - 1];

    // Verify that the ad function at the boundary is indeed the API we are
    // tracking. This prevents misidentifying unrelated calls (e.g., a non-ad
    // script calling a random helper function inside an ad) as a monkey patch.
    // If the boundary function doesn't match the API, it's not the pattern we
    // are looking for.
    return api_function == ad_barrier_frame.function;
  }

  // If the loop completes, the entire stack trace is from ad scripts, so the
  // call did not originate from a non-ad script.
  return false;
}

bool AdTracker::IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                                       const v8::Local<v8::Function>& function,
                                       MonkeyPatchableApi api) const {
  // 1. Get the implementation of `api` from the v8 context.
  ApiFunctionInfo api_info = GetApiFunctionInfo(isolate, api);
  if (!api_info.is_monkey_patched) {
    return false;
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Function> api_function;
  if (!api_info.function.ToLocal(&api_function)) {
    return false;
  }

  // 2. If the `api` is monkeypatched, see if it matches `function`.
  return function == api_function;
}

bool AdTracker::IsKnownAdScript(ExecutionContext* execution_context,
                                const String& url) {
  if (!execution_context) {
    return false;
  }

  if (IsKnownAdExecutionContext(execution_context)) {
    return true;
  }

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
    V8ScriptId script_id) {
  DCHECK(!script_name.empty());
  DCHECK_NE(v8::Message::kNoScriptIdInfo, script_id.value());
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

AdTracker::AdScriptAncestry AdTracker::GetAncestry(V8ScriptId script_id) {
  AdTracker::AdScriptAncestry ancestry;

  // TODO(yaoxia): Determine if we should CHECK that that the script ID in each
  // step is guaranteed to be present in `ad_script_data_`.
  auto provenance_it = ad_script_data_.find(script_id);
  if (provenance_it == ad_script_data_.end()) {
    return ancestry;
  }

  HashSet<V8ScriptId> seen_script_ids;
  bool duplicate = false;

  ancestry.ancestry_chain.push_back(provenance_it->value.id);
  seen_script_ids.insert(provenance_it->value.id.id);

  while (provenance_it != ad_script_data_.end()) {
    const AdProvenance& ad_provenance = provenance_it->value.provenance;

    // Update `ancestry` based on the type of the `ad_provenance` variant.
    bool root_reached = std::visit(
        absl::Overload{[&](NoProvenance) { return true; },
                       [&](V8ScriptId script_id) {
                         // Prevent an infinite loop due to cycles.
                         if (!seen_script_ids.insert(script_id).is_new_entry) {
                           duplicate = true;
                           return true;
                         }

                         auto it = this->ad_script_data_.find(script_id);
                         if (it == this->ad_script_data_.end()) {
                           // This can happen if an element is moved from one
                           // AdTracker to another, and it references a script
                           // id that this tracker doesn't know about.
                           return true;
                         }
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
