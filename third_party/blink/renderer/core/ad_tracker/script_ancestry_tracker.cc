// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"

#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "v8/include/v8-inspector.h"

namespace blink {

namespace {

v8_inspector::V8DebuggerId GetDebuggerIdForCurrentContext(
    v8::Isolate* isolate) {
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
  CHECK(!v8_context.IsEmpty());
  int context_id = v8_inspector::V8ContextInfo::executionContextId(v8_context);
  return ThreadDebugger::From(isolate)->GetV8Inspector()->uniqueDebuggerId(
      context_id);
}

String GenerateFakeUrlFromScriptId(V8ScriptId script_id) {
  // Null string is used to represent scripts with neither a name nor an ID.
  if (script_id.value() == v8::Message::kNoScriptIdInfo) {
    return String();
  }

  // The prefix cannot appear in real URLs.
  return Format("{{ id {} }}", script_id.value());
}

}  // namespace

ScriptAncestryTracker::ScriptAncestryTracker(LocalFrame* local_root,
                                             ScriptInitiationMonitor* monitor)
    : local_root_(local_root), monitor_(monitor) {
  CHECK(local_root_);
  if (monitor_) {
    monitor_->AddObserver(this);
  }
}

ScriptAncestryTracker::~ScriptAncestryTracker() = default;

v8::Isolate* ScriptAncestryTracker::GetIsolate() const {
  return monitor_ ? monitor_->GetIsolate() : v8::Isolate::TryGetCurrent();
}

void ScriptAncestryTracker::WillExecuteScript(
    ExecutionContext& execution_context,
    V8ScriptId script_id,
    const String& script_url,
    LazyStackTrace& stack_trace) {
  running_sync_tasks_++;

  if (script_id.value() <= 0) {
    return;
  }

  // We're executing a script's top-level. This is our first time seeing the
  // script id for the given url.
  // Note: V8 occasionally reports the document's URL instead of an empty string
  // for inline scripts or evals. We check for both to accurately identify them
  // as inline, ensuring they are assigned a synthetic ID and correctly linked
  // to their initiating parent in the compilation graph.
  bool is_inline_script =
      script_url.empty() || script_url == execution_context.Url().GetString();
  String url =
      is_inline_script ? GenerateFakeUrlFromScriptId(script_id) : script_url;

  std::optional<V8ScriptId> marked_script_id = GetMarkedScriptInStack(
      StackType::kTopOnly, stack_trace,
      /*ignore_monkey_patch=*/MonkeyPatchableApi::kNodeAppendChild);

  // Since this is our first time running the script, this is the first we've
  // seen of its script id. Record the id so that we can refer to the script
  // by id rather than string.
  if (!GetScriptMetadata(script_id)) {
    v8_inspector::V8DebuggerId debugger_id =
        GetDebuggerIdForCurrentContext(execution_context.GetIsolate());
    script_metadata_.insert(
        script_id,
        ScriptMetadata{debugger_id, marked_script_id.value_or(V8ScriptId()),
                       url});
  }

  OnScriptRegistered(execution_context, script_id, url, marked_script_id);

  if (!bottom_most_script_.has_value() && IsMarkedScript(script_id)) {
    bottom_most_script_ = script_id;
  }
}

void ScriptAncestryTracker::DidExecuteScript(V8ScriptId script_id) {
  running_sync_tasks_--;

  if (bottom_most_script_.has_value() &&
      bottom_most_script_.value() == script_id) {
    bottom_most_script_.reset();
  }

  if (running_sync_tasks_ == 0) {
    monkey_patch_calls_in_scope_.clear();
  }
}

void ScriptAncestryTracker::DidRegisterDynamicScript(
    V8ScriptId script_id,
    LazyStackTrace& stack_trace) {
  std::optional<V8ScriptId> marked_script_id;

  // If there was a marked script on the async stack when the script was first
  // written, use that state regardless of what's on the sync stack.
  if (!async_script_stack_.empty() && async_script_stack_.back().has_value()) {
    marked_script_id = async_script_stack_.back();
  } else {
    marked_script_id = GetMarkedScriptInStack(
        StackType::kTopOnly, stack_trace,
        /*ignore_monkey_patch=*/MonkeyPatchableApi::kNodeAppendChild);
  }

  RegisterScript(script_id, marked_script_id);
}

void ScriptAncestryTracker::WillCallFunction(
    ExecutionContext& execution_context,
    V8ScriptId script_id,
    bool is_nested,
    LazyStackTrace& stack_trace) {
  running_sync_tasks_++;

  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (is_nested || script_id.value() <= 0) {
    return;
  }

  if (!bottom_most_script_.has_value() && IsMarkedScript(script_id)) {
    bottom_most_script_ = script_id;
  }
}

void ScriptAncestryTracker::DidCallFunction(V8ScriptId script_id,
                                            bool is_nested) {
  running_sync_tasks_--;

  if (is_nested) {
    return;
  }
  if (bottom_most_script_.has_value() &&
      bottom_most_script_.value() == script_id) {
    bottom_most_script_.reset();
  }

  if (running_sync_tasks_ == 0) {
    monkey_patch_calls_in_scope_.clear();
  }
}

void ScriptAncestryTracker::DidCreateAsyncTask(
    probe::AsyncTaskContext* task_context,
    LazyStackTrace& stack_trace) {
  DCHECK(task_context);
  v8::Isolate* isolate = GetIsolate();
  if (isolate && !isolate->GetCurrentContext().IsEmpty()) {
    v8::HandleScope handle_scope(isolate);
    // TODO(jkarlin): Restrict the kNodeAppendChild monkeypatch exception
    // specifically to script element creation tasks (e.g., PendingScript) by
    // passing the async task name/type to DidCreateAsyncTask.
    // Note: We do this check separately from GetMarkedScriptInStack because we
    // don't want to count it against the 1-call-per-sync-stack budget in case
    // downstream callers want to call GetMarkedScriptInStack.
    if (WasApiCalledByNonAttributedScript(
            isolate, MonkeyPatchableApi::kNodeAppendChild, stack_trace)) {
      return;
    }
  }

  if (std::optional<V8ScriptId> script_id =
          GetMarkedScriptInStack(StackType::kTopOnly, stack_trace)) {
    task_context->SetMarkedScript(GetTrackerType(), *script_id);
  }
}

void ScriptAncestryTracker::DidStartAsyncTask(
    probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  async_script_stack_.emplace_back(
      task_context->GetMarkedScript(GetTrackerType()));
}

void ScriptAncestryTracker::DidFinishAsyncTask(
    probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  async_script_stack_.pop_back();
}

void ScriptAncestryTracker::DidCreateFrame(LocalFrame* frame,
                                           LazyStackTrace& stack_trace) {
  if (!frame || frame->IsMainFrame()) {
    return;
  }

  // If the parent frame was created by a marked script, inherit that status.
  if (const Frame* parent = frame->Tree().Parent()) {
    if (const auto* local_parent = DynamicTo<LocalFrame>(parent)) {
      if (ScriptAncestryTracker::IsMarkedFrame(local_parent)) {
        marked_frames_.insert(frame, GetInitiatingScriptId(local_parent));
        return;
      }
    }
  }

  // Note: For initial documents and same-origin frames created synchronously
  // by script (e.g. appendChild or srcdoc), the creating script is on the
  // stack. Cross-origin OOPIFs and remote swaps do not carry initiating V8
  // script state across process boundaries without explicit browser-side IPC
  // propagation.
  std::optional<V8ScriptId> marked_script_id = GetMarkedScriptInStack(
      StackType::kTopOnly, stack_trace,
      /*ignore_monkey_patch=*/MonkeyPatchableApi::kNodeAppendChild);
  if (!marked_script_id.has_value()) {
    return;
  }

  marked_frames_.insert(frame, *marked_script_id);
}

void ScriptAncestryTracker::DidSwapFrame(LocalFrame* old_frame,
                                         LocalFrame* new_frame) {
  if (!old_frame || !new_frame) {
    return;
  }
  auto it = marked_frames_.find(old_frame);
  if (it != marked_frames_.end()) {
    V8ScriptId initiating_id = it->value;
    marked_frames_.erase(it);
    marked_frames_.insert(new_frame, initiating_id);
  }
}

bool ScriptAncestryTracker::IsMarkedFrame(const LocalFrame* frame) const {
  if (!frame) {
    return false;
  }
  return marked_frames_.Contains(const_cast<LocalFrame*>(frame));
}

V8ScriptId ScriptAncestryTracker::GetInitiatingScriptId(
    const LocalFrame* frame) const {
  if (!frame) {
    return V8ScriptId();
  }
  auto it = marked_frames_.find(const_cast<LocalFrame*>(frame));
  return it != marked_frames_.end() ? it->value : V8ScriptId();
}

bool ScriptAncestryTracker::IsMarkedExecutionContext(
    ExecutionContext* execution_context) const {
  if (!execution_context) {
    return false;
  }
  // TODO(jkarlin): Do the same check for worker contexts.
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    return IsMarkedFrame(window->GetFrame());
  }
  return false;
}

std::optional<V8ScriptId> ScriptAncestryTracker::GetMarkedScriptInStack(
    StackType stack_type,
    LazyStackTrace& stack_trace,
    MonkeyPatchableApi ignore_monkey_patch) {
  // If the currently executing context is within a frame created by a marked
  // script (or marked frame), classify the stack as marked and return the
  // creating script's id. Note: Execution context inside a marked frame
  // intentionally takes precedence over `kBottomOnly` bottom-of-stack checks,
  // matching existing ad frame semantics.
  if (HasMarkedFrames()) {
    if (v8::Isolate* isolate = GetIsolate()) {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
      if (!v8_context.IsEmpty()) {
        if (auto* window =
                DynamicTo<LocalDOMWindow>(ToExecutionContext(v8_context))) {
          if (const LocalFrame* frame = window->GetFrame();
              IsMarkedFrame(frame)) {
            V8ScriptId initiating_id = GetInitiatingScriptId(frame);
            // Return the initiating script ID if present, or an empty
            // V8ScriptId (engaged in std::optional) to classify execution
            // inside a marked frame as marked even without a specific creating
            // script ID.
            return initiating_id.value() > 0 ? initiating_id : V8ScriptId();
          }
        }
      }
    }
  }

  if (stack_type == StackType::kBottomOnly) {
    if (bottom_most_script_.has_value() &&
        IsMarkedScript(*bottom_most_script_)) {
      return *bottom_most_script_;
    }
    for (auto& script : async_script_stack_) {
      if (script.has_value() && IsMarkedScript(*script)) {
        return *script;
      }
    }
    return std::nullopt;
  }

  if (script_metadata_.empty()) {
    return std::nullopt;
  }

  v8::Isolate* isolate = GetIsolate();

  // When the `ignore_monkey_patch` heuristic is specified, we inspect the top
  // five stack frames instead of just the top frame. It allows us to capture
  // publisher monkey patch scenarios (i.e., one or more publisher monkey
  // patches that passively invoke a marked script's intent).
  size_t limit = (ignore_monkey_patch != MonkeyPatchableApi::kNone) ? 5 : 1;
  auto stack = stack_trace.GetStack(limit);

  if (stack.empty()) {
    if (!async_script_stack_.empty() &&
        async_script_stack_.back().has_value()) {
      V8ScriptId script_id = *async_script_stack_.back();
      if (IsMarkedScript(script_id)) {
        return script_id;
      }
    }
    return std::nullopt;
  }

  std::optional<V8ScriptId> matched_script;
  int matched_script_index = -1;

  for (size_t i = 0; i < stack.size(); ++i) {
    V8ScriptId script_id(stack[i].id);
    if (script_id.value() <= 0) {
      return std::nullopt;
    }

    if (IsMarkedScript(script_id)) {
      matched_script_index = static_cast<int>(i);
      matched_script = script_id;
      break;
    }
  }

  if (!matched_script.has_value()) {
    // The top scripts on the stack are not marked scripts belonging to this
    // domain.
    //
    // If the top scripts on the stack are non-marked, then we consider the
    // stack to be non-marked related, as publisher script may be running an
    // event callback.
    return std::nullopt;
  }

  if (ignore_monkey_patch != MonkeyPatchableApi::kNone) {
    MonkeyPatchableApiFunctionInfo api_info =
        GetMonkeyPatchableApiFunctionInfo(isolate, ignore_monkey_patch);
    v8::Local<v8::Function> api_function;
    if (api_info.is_monkey_patched &&
        api_info.function.ToLocal(&api_function)) {
      MonkeyPatchCallerResult result =
          FindMonkeyPatchCaller(isolate, api_function, stack);
      if (result.is_api_in_stack) {
        if (result.marked_caller_id.has_value()) {
          // The API was invoked by a marked script. Attribute the call to it.
          return result.marked_caller_id;
        } else {
          // The API was invoked by a non-marked script. Allow this to bypass
          // the marked-script-in-stack check once per synchronous task.
          if (IsFirstMonkeyPatchCall(ignore_monkey_patch)) {
            return std::nullopt;
          }
        }
      }
    }
  }

  // If the API was not found in the stack (e.g., cached patch or exceeds the
  // stack scan limit), fall back to the top stack frame's status.

  // Top script is non-marked.
  if (matched_script_index > 0) {
    return std::nullopt;
  }

  // Top script is marked. Attribute to marked script.
  return matched_script;
}

void ScriptAncestryTracker::Shutdown() {
  marked_frames_.clear();
  script_metadata_.clear();
  async_script_stack_.clear();
  bottom_most_script_.reset();
  monkey_patch_calls_in_scope_.clear();
  if (monitor_) {
    monitor_->RemoveObserver(this);
    monitor_ = nullptr;
  }
  local_root_ = nullptr;
}

void ScriptAncestryTracker::Trace(Visitor* visitor) const {
  ScriptInitiationMonitor::Observer::Trace(visitor);
  visitor->Trace(local_root_);
  visitor->Trace(monitor_);
  visitor->Trace(marked_frames_);
}

void ScriptAncestryTracker::RegisterScript(
    V8ScriptId script_id,
    std::optional<V8ScriptId> marked_script_id) {
  DCHECK_NE(v8::Message::kNoScriptIdInfo, script_id.value());
  String script_name = GenerateFakeUrlFromScriptId(script_id);

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8_inspector::V8DebuggerId debugger_id =
      GetDebuggerIdForCurrentContext(isolate);

  script_metadata_.insert(
      script_id,
      ScriptMetadata{debugger_id, marked_script_id.value_or(V8ScriptId()),
                     script_name});

  OnScriptRegistered(CHECK_DEREF(CurrentExecutionContext(isolate)), script_id,
                     script_name, marked_script_id);
}

const ScriptAncestryTracker::ScriptMetadata*
ScriptAncestryTracker::GetScriptMetadata(V8ScriptId script_id) const {
  auto it = script_metadata_.find(script_id);
  return it != script_metadata_.end() ? &it->value : nullptr;
}

bool ScriptAncestryTracker::IsFirstMonkeyPatchCall(MonkeyPatchableApi api) {
  // The heuristic only applies on the first call to an API within a task.
  // Note, running_sync_tasks_ will be 0 when in a promise callback microtask,
  // since we're not monitoring promises we don't apply the allow-once heuristic
  // in that scenario.
  if (running_sync_tasks_ == 0) {
    return true;
  }
  if (monkey_patch_calls_in_scope_.Contains(api)) {
    return false;
  }
  monkey_patch_calls_in_scope_.insert(api);
  return true;
}

ScriptAncestryTracker::MonkeyPatchCallerResult
ScriptAncestryTracker::FindMonkeyPatchCaller(
    v8::Isolate* isolate,
    const v8::Local<v8::Function>& api_function,
    base::span<const v8::StackTrace::ScriptData> stack) const {
  MonkeyPatchCallerResult result;
  if (stack.size() <= 1) {
    return result;
  }

  // Look for the exact tracked API in the stack. The frame immediately
  // preceding it is the true initiator.
  for (size_t i = 0; i < stack.size() - 1; ++i) {
    if (IsFunctionAMonkeyPatch(isolate, stack[i].function, api_function)) {
      result.is_api_in_stack = true;
      V8ScriptId caller_id(stack[i + 1].id);
      if (caller_id.value() > 0 && IsMarkedScript(caller_id)) {
        result.marked_caller_id = caller_id;
      }
      return result;
    }
  }

  return result;
}

bool ScriptAncestryTracker::WasApiCalledByNonAttributedScript(
    v8::Isolate* isolate,
    MonkeyPatchableApi api,
    LazyStackTrace& /*stack_trace*/) const {
  MonkeyPatchableApiFunctionInfo api_info =
      GetMonkeyPatchableApiFunctionInfo(isolate, api);
  if (!api_info.is_monkey_patched) {
    return false;
  }

  v8::Local<v8::Function> api_function;
  if (!api_info.function.ToLocal(&api_function)) {
    return false;
  }

  LazyStackTrace stack_trace(isolate);
  auto stack = stack_trace.GetStack(5);

  MonkeyPatchCallerResult result =
      FindMonkeyPatchCaller(isolate, api_function, stack);
  if (result.is_api_in_stack) {
    return !result.marked_caller_id.has_value();
  }

  return false;
}

}  // namespace blink
