// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

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
      ThreadDebugger::From(v8_context->GetIsolate());
  DCHECK(thread_debugger);
  v8_inspector::V8Inspector* inspector = thread_debugger->GetV8Inspector();
  DCHECK(inspector);
  return inspector->uniqueDebuggerId(contextId);
}

}  // namespace

// static
AdTracker* AdTracker::FromExecutionContext(
    ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;
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

String AdTracker::ScriptAtTopOfStack(
    std::optional<AdScriptIdentifier>* out_top_script) {
  // CurrentStackTrace is 10x faster than CaptureStackTrace if all that you need
  // is the url of the script at the top of the stack. See crbug.com/1057211 for
  // more detail.
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (!isolate) [[unlikely]] {
    return String();
  }

  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/1);
  if (stack_trace.IsEmpty() || stack_trace->GetFrameCount() < 1)
    return String();

  v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, 0);
  v8::Local<v8::String> script_name = frame->GetScriptName();

  if (out_top_script) {
    *out_top_script = AdScriptIdentifier(
        GetDebuggerIdForContext(isolate->GetCurrentContext()),
        frame->GetScriptId());
  }

  if (script_name.IsEmpty() || !script_name->Length())
    return GenerateFakeUrlFromScriptId(frame->GetScriptId());

  return ToCoreString(isolate, script_name);
}

ExecutionContext* AdTracker::GetCurrentExecutionContext() {
  // Determine the current ExecutionContext.
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (!isolate) {
    return nullptr;
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return context.IsEmpty() ? nullptr : ToExecutionContext(context);
}

void AdTracker::WillExecuteScript(ExecutionContext* execution_context,
                                  const v8::Local<v8::Context>& v8_context,
                                  const String& script_url,
                                  int script_id,
                                  bool top_level_execution) {
  bool is_inline_script =
      script_url.empty() && script_id != v8::Message::kNoScriptIdInfo;

  String url =
      is_inline_script ? GenerateFakeUrlFromScriptId(script_id) : script_url;

  bool is_ad = IsKnownAdScript(execution_context, url);

  // On first run of a script we do some additional checks and bookkeeping.
  if (top_level_execution) {
    // For inline scripts, this is our opportunity to check the stack to see if
    // an ad created it since inline scripts are run immediately.
    std::optional<AdScriptIdentifier> ancestor_ad_script;
    if (!is_ad && is_inline_script &&
        IsAdScriptInStackHelper(StackType::kBottomAndTop,
                                &ancestor_ad_script)) {
      std::unique_ptr<AdProvenance> ad_provenance;
      if (ancestor_ad_script.has_value()) {
        ad_provenance =
            std::make_unique<AdAncestorProvenance>(*ancestor_ad_script);
      } else {
        // This can happen if the script originates from an ad context without
        // further traceable script (crbug.com/421202278).
        ad_provenance = std::make_unique<NoAdProvenance>();
      }

      AppendToKnownAdScripts(*execution_context, url, std::move(ad_provenance));
      is_ad = true;
    }

    // Since this is our first time running the script, this is the first we've
    // seen of its script id. Record the id so that we can refer to the script
    // by id rather than string.
    if (is_ad && !url.empty() &&
        !IsKnownAdExecutionContext(execution_context)) {
      OnScriptIdAvailableForKnownAdScript(execution_context, v8_context, url,
                                          script_id);
    }
  }

  stack_frame_is_ad_.push_back(is_ad);
  if (is_ad) {
    if (num_ads_in_stack_ == 0) {
      // Stash the first ad script on the stack.
      bottom_most_ad_script_ =
          AdScriptIdentifier(GetDebuggerIdForContext(v8_context), script_id);
    }
    num_ads_in_stack_ += 1;
  }
}

void AdTracker::DidExecuteScript() {
  if (stack_frame_is_ad_.back()) {
    DCHECK_LT(0, num_ads_in_stack_);
    num_ads_in_stack_ -= 1;
    if (num_ads_in_stack_ == 0)
      bottom_most_ad_script_.reset();
  }
  stack_frame_is_ad_.pop_back();
}

void AdTracker::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.context, probe.v8_context, probe.script_url,
                    probe.script_id, /*top_level_execution=*/true);
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
  if (!resource_name.IsEmpty()) {
    v8::Isolate* isolate = ToIsolate(local_root_);
    v8::MaybeLocal<v8::String> resource_name_string =
        resource_name->ToString(isolate->GetCurrentContext());
    // Rarely, ToString() can return an empty result, even if |resource_name|
    // isn't empty (crbug.com/1086832).
    if (!resource_name_string.IsEmpty())
      script_url = ToCoreString(isolate, resource_name_string.ToLocalChecked());
  }
  WillExecuteScript(probe.context, probe.v8_context, script_url,
                    probe.function->ScriptId(), /*top_level_execution=*/false);
}

void AdTracker::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;

  DidExecuteScript();
}

bool AdTracker::CalculateIfAdSubresource(
    ExecutionContext* execution_context,
    const KURL& request_url,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info,
    bool known_ad,
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
  known_ad = known_ad || IsAdScriptInStackHelper(StackType::kBottomAndTop,
                                                 &ancestor_ad_script);

  // If it is a script marked as an ad and it's not in an ad context, append it
  // to the known ad script set. We don't need to keep track of ad scripts in ad
  // contexts, because any script executed inside an ad context is considered an
  // ad script by IsKnownAdScript.
  if (resource_type == ResourceType::kScript && known_ad &&
      !is_ad_execution_context) {
    DCHECK(!ancestor_ad_script || !rule.IsValid());

    std::unique_ptr<AdProvenance> ad_provenance;
    if (!ancestor_ad_script && !rule.IsValid()) {
      ad_provenance = std::make_unique<NoAdProvenance>();
    } else if (ancestor_ad_script) {
      ad_provenance =
          std::make_unique<AdAncestorProvenance>(*ancestor_ad_script);
    } else {
      DCHECK(rule.IsValid());
      ad_provenance = std::make_unique<AdRulesetProvenance>(rule);
    }

    AppendToKnownAdScripts(*execution_context, request_url.GetString(),
                           std::move(ad_provenance));
  }

  return known_ad;
}

void AdTracker::DidCreateAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  std::optional<AdScriptIdentifier> id;
  if (IsAdScriptInStackHelper(StackType::kBottomAndTop, &id)) {
    task_context->SetAdTask(id);
  }
}

void AdTracker::DidStartAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  if (task_context->IsAdTask()) {
    if (running_ad_async_tasks_ == 0) {
      DCHECK(!bottom_most_async_ad_script_.has_value());
      bottom_most_async_ad_script_ = task_context->ad_identifier();
    }

    running_ad_async_tasks_ += 1;
  }
}

void AdTracker::DidFinishAsyncTask(probe::AsyncTaskContext* task_context) {
  DCHECK(task_context);
  if (task_context->IsAdTask()) {
    DCHECK_GE(running_ad_async_tasks_, 1);
    running_ad_async_tasks_ -= 1;
    if (running_ad_async_tasks_ == 0)
      bottom_most_async_ad_script_.reset();
  }
}

bool AdTracker::IsAdScriptInStack(StackType stack_type,
                                  AdScriptAncestry* out_ad_script_ancestry) {
  std::optional<AdScriptIdentifier> out_ad_script;

  std::optional<AdScriptIdentifier>* out_ad_script_ptr =
      out_ad_script_ancestry ? &out_ad_script : nullptr;

  bool is_ad_script_in_stack =
      IsAdScriptInStackHelper(stack_type, out_ad_script_ptr);

  if (out_ad_script.has_value()) {
    CHECK(out_ad_script_ancestry);
    CHECK(is_ad_script_in_stack);
    *out_ad_script_ancestry = GetAncestry(out_ad_script.value());
  }

  return is_ad_script_in_stack;
}

bool AdTracker::IsAdScriptInStackHelper(
    StackType stack_type,
    std::optional<AdScriptIdentifier>* out_ad_script) {
  // First check if async tasks are running, as `bottom_most_async_ad_script_`
  // is more likely to be what the caller is looking for than
  // `bottom_most_ad_script_`.
  if (running_ad_async_tasks_ > 0) {
    if (out_ad_script)
      *out_ad_script = bottom_most_async_ad_script_;
    return true;
  }

  if (num_ads_in_stack_ > 0) {
    if (out_ad_script)
      *out_ad_script = bottom_most_ad_script_;
    return true;
  }

  ExecutionContext* execution_context = GetCurrentExecutionContext();
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

  if (stack_type == StackType::kBottomOnly)
    return false;

  // The stack scanned by the AdTracker contains entry points into the stack
  // (e.g., when v8 is executed) but not the entire stack. For a small cost we
  // can also check the top of the stack (this is much cheaper than getting the
  // full stack from v8).
  return IsKnownAdScriptForCheckedContext(*execution_context, String(),
                                          out_ad_script);
}

bool AdTracker::IsKnownAdScript(ExecutionContext* execution_context,
                                const String& url) {
  if (!execution_context)
    return false;

  if (IsKnownAdExecutionContext(execution_context))
    return true;

  // We don't care about the `out_ad_script` param here because that only gets
  // filled when `url` is empty, but we have a url to pass in this case.
  return IsKnownAdScriptForCheckedContext(*execution_context, url,
                                          /*out_ad_script=*/nullptr);
}

bool AdTracker::IsKnownAdScriptForCheckedContext(
    ExecutionContext& execution_context,
    const String& url,
    std::optional<AdScriptIdentifier>* out_ad_script) {
  DCHECK(!IsKnownAdExecutionContext(&execution_context));
  auto it = context_known_ad_scripts_.find(&execution_context);
  if (it == context_known_ad_scripts_.end()) {
    return false;
  }

  if (it->value.empty()) {
    return false;
  }

  std::optional<AdScriptIdentifier> top_of_stack_script;
  // Delay calling ScriptAtTopOfStack() as much as possible due to its cost.
  String script_url =
      url.IsNull()
          ? ScriptAtTopOfStack(out_ad_script ? &top_of_stack_script : nullptr)
          : url;

  if (script_url.empty()) {
    return false;
  }

  bool found = it->value.Contains(script_url);
  if (found && out_ad_script) {
    *out_ad_script = std::move(top_of_stack_script);
  }

  return found;
}

// This is a separate function for testing purposes.
void AdTracker::AppendToKnownAdScripts(
    ExecutionContext& execution_context,
    const String& url,
    std::unique_ptr<AdProvenance> ad_provenance) {
  DCHECK(!url.empty());
  DCHECK(ad_provenance);

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
  auto it = context_known_ad_scripts_.find(execution_context);
  DCHECK(it != context_known_ad_scripts_.end());

  // Skip linking if the current script has no script ID. This avoids
  // introducing cycles within the `ancestor_ad_scripts_` graph.
  if (script_id == v8::Message::kNoScriptIdInfo) {
    return;
  }

  const HashMap<String, std::unique_ptr<AdProvenance>>&
      known_ad_scripts_and_provenance = it->value;

  auto known_ad_script_it = known_ad_scripts_and_provenance.find(script_name);
  DCHECK(known_ad_script_it != known_ad_scripts_and_provenance.end());

  const std::unique_ptr<AdProvenance>& ad_provenance =
      known_ad_script_it->value;
  DCHECK(ad_provenance);

  // We clone `ad_provenance` rather than transferring ownership. This is
  // because multiple script executions might originate from the same script
  // URL, and are intended to share the same provenance. While this approach
  // might not perfectly mirror the script loading ancestry in all complex
  // scenarios, it's considered sufficient for our tracking purposes.
  AdScriptIdentifier current_ad_script =
      AdScriptIdentifier(GetDebuggerIdForContext(v8_context), script_id);

  ad_script_provenances_.insert(current_ad_script, ad_provenance->Clone());
}

AdTracker::AdScriptAncestry AdTracker::GetAncestry(
    const AdScriptIdentifier& ad_script) {
  AdTracker::AdScriptAncestry ancestry;
  ancestry.ancestry_chain.push_back(ad_script);

  // Limits the ancestry chain length to protect against potential cycles in the
  // ancestry graph (though unexpected).
  constexpr size_t kMaxScriptAncestrySize = 50;
  bool max_size_reached = false;

  // TODO(yaoxia): Determine if we should CHECK that that the script ID in each
  // step is guaranteed to be present in `ad_script_provenances_`.
  auto provenance_it =
      ad_script_provenances_.find(ancestry.ancestry_chain.back());
  while (provenance_it != ad_script_provenances_.end()) {
    const std::unique_ptr<AdProvenance>& ad_provenance = provenance_it->value;

    bool root_reached = false;
    switch (ad_provenance->Type()) {
      case AdProvenance::ProvenanceType::kMatchedRule: {
        ancestry.root_script_filterlist_rule =
            DynamicTo<AdRulesetProvenance>(*ad_provenance)->filterlist_rule;
        root_reached = true;
        break;
      }
      case AdProvenance::ProvenanceType::kAncestorScript: {
        ancestry.ancestry_chain.push_back(
            DynamicTo<AdAncestorProvenance>(*ad_provenance)
                ->ancestor_ad_script);
        break;
      }
      case AdProvenance::ProvenanceType::kNone: {
        root_reached = true;
        break;
      }
    }

    if (ancestry.ancestry_chain.size() >= kMaxScriptAncestrySize) {
      max_size_reached = true;
      break;
    }

    if (root_reached) {
      break;
    }

    provenance_it = ad_script_provenances_.find(ancestry.ancestry_chain.back());
  }

  base::UmaHistogramBoolean(
      "Navigation.IframeCreated.AdTracker.MaxScriptAncestrySizeReached",
      max_size_reached);

  return ancestry;
}

void AdTracker::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(context_known_ad_scripts_);
}

}  // namespace blink
