// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"

#include <optional>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

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

}  // namespace

String AdTracker::AdScriptAncestry::ToString() const {
  if (ancestry_chain.empty() || !root_script_filterlist_rule.IsValid()) {
    return String();
  }

  StringBuilder builder;
  builder.Append("Debug info: adscript '");
  builder.Append(ancestry_chain[0].name);
  builder.Append("' ");
  for (wtf_size_t i = 1; i < ancestry_chain.size(); ++i) {
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
  if (!execution_context) {
    return nullptr;
  }
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return nullptr;
  }
  LocalFrame* frame = window->GetFrame();
  return frame ? frame->GetAdTracker() : nullptr;
}

// static
bool AdTracker::IsAdScriptExecutingInDocument(Document* document,
                                              StackType stack_type) {
  AdTracker* ad_tracker =
      document->GetFrame() ? document->GetFrame()->GetAdTracker() : nullptr;
  return ad_tracker && ad_tracker->IsAdScriptInStack(stack_type);
}

AdTracker::AdTracker(LocalFrame* local_root, ScriptInitiationMonitor* monitor)
    : ScriptAncestryTracker(local_root, monitor) {}

AdTracker::~AdTracker() = default;

// Resource fetchers call `CalculateIfAdSubresource` to determine if any
// resource (not just scripts) are ad related. This method checks if the
// resource was already tagged by the subresource filter, if the execution
// context is a known ad context, or if any executing script on the stack is an
// ad script.
std::optional<AdProvenance> AdTracker::CalculateIfAdSubresource(
    ExecutionContext* execution_context,
    const KURL& request_url,
    ResourceType resource_type,
    const FetchInitiatorInfo& initiator_info,
    std::optional<AdProvenance> known_ad_provenance,
    bool scan_javascript_stack) {
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
  if (!known_ad_provenance && scan_javascript_stack) {
    v8::Isolate* isolate =
        execution_context ? execution_context->GetIsolate() : nullptr;
    if (isolate) {
      v8::HandleScope handle_scope(isolate);
      LazyStackTrace stack_trace(isolate);
      std::optional<V8ScriptId> ancestor_ad_script;

      if (IsMarkedScriptInStack(
              StackType::kTopOnly, stack_trace, &ancestor_ad_script,
              /*ignore_monkey_patch=*/MonkeyPatchableApi::kNodeAppendChild) &&
          ancestor_ad_script.has_value()) {
        known_ad_provenance = AdProvenance(*ancestor_ad_script);
      }
    }
  }

  // If it is a script marked as an ad and it's not in an ad context, append it
  // to the known ad script set.
  if (resource_type == ResourceType::kScript && known_ad_provenance &&
      execution_context && !is_ad_execution_context) {
    if (!request_url.IsEmpty()) {
      auto result = context_known_ad_scripts_.insert(
          execution_context, KnownAdScriptsAndProvenance());
      // While technically the same script URL can be loaded with different
      // provenances (e.g., from different ancestors), we track only the first
      // association for simplicity.
      result.stored_value->value.insert(request_url.GetString(),
                                        *known_ad_provenance);
    }
  }

  return known_ad_provenance;
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

AdTracker::AdScriptAncestry AdTracker::GetAncestry(V8ScriptId script_id) {
  AdTracker::AdScriptAncestry ancestry;

  auto it = ad_scripts_.find(script_id);
  if (it == ad_scripts_.end()) {
    return ancestry;
  }

  const ScriptAncestryTracker::ScriptMetadata* metadata =
      GetScriptMetadata(script_id);
  if (!metadata) {
    return ancestry;
  }

  HashSet<V8ScriptId> seen_script_ids;
  bool duplicate = false;

  ancestry.ancestry_chain.emplace_back(metadata->context_id, script_id,
                                       metadata->url);
  seen_script_ids.insert(script_id);

  AdProvenance ad_provenance = it->value;
  while (true) {
    bool root_reached = std::visit(
        absl::Overload{
            [&](NoProvenance) { return true; },
            [&](V8ScriptId marked_script_id) {
              // Prevent an infinite loop due to cycles.
              if (!seen_script_ids.insert(marked_script_id).is_new_entry) {
                duplicate = true;
                return true;
              }
              const ScriptAncestryTracker::ScriptMetadata* parent_metadata =
                  GetScriptMetadata(marked_script_id);
              if (!parent_metadata) {
                // This can happen if an element is moved from one AdTracker to
                // another, and it references a script id that this tracker
                // doesn't know about.
                return true;
              }
              ancestry.ancestry_chain.emplace_back(parent_metadata->context_id,
                                                   marked_script_id,
                                                   parent_metadata->url);

              auto parent_it = ad_scripts_.find(marked_script_id);
              if (parent_it == ad_scripts_.end()) {
                return true;
              }
              ad_provenance = parent_it->value;
              // Move on to the next ancestor.
              return false;
            },
            [&](const subresource_filter::ScopedRule& rule) {
              ancestry.root_script_filterlist_rule = rule;
              // We've reached the ruleset rule which is our "root", so stop.
              return true;
            }},
        ad_provenance);

    if (root_reached) {
      break;
    }
  }

  base::UmaHistogramBoolean(
      "Navigation.IframeCreated.AdTracker.DuplicateAncestryScriptId",
      duplicate);

  return ancestry;
}

bool AdTracker::IsAdScriptInStack(StackType stack_type,
                                  MonkeyPatchableApi ignore_monkey_patch,
                                  AdScriptAncestry* out_ad_script_ancestry) {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (isolate) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    ExecutionContext* execution_context =
        context.IsEmpty() ? nullptr : ToExecutionContext(context);
    if (execution_context && IsKnownAdExecutionContext(execution_context)) {
      if (out_ad_script_ancestry) {
        *out_ad_script_ancestry = AdScriptAncestry();
        if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
          if (LocalFrame* frame = window->GetFrame()) {
            std::optional<AdScriptIdentifier> creation_script =
                frame->CreationAdScript();
            if (creation_script.has_value() &&
                creation_script->id != AdScriptIdentifier::kEmptyId) {
              *out_ad_script_ancestry = GetAncestry(creation_script->id);
            }
          }
        }
      }
      return true;
    }
  }

  std::optional<V8ScriptId> out_ad_script;
  std::optional<v8::HandleScope> handle_scope;
  if (isolate) {
    handle_scope.emplace(isolate);
  }
  LazyStackTrace stack_trace(isolate);
  bool is_ad_script_in_stack = IsMarkedScriptInStack(
      stack_type, stack_trace, &out_ad_script, ignore_monkey_patch);

  if (out_ad_script_ancestry) {
    *out_ad_script_ancestry = AdScriptAncestry();
    if (out_ad_script.has_value()) {
      *out_ad_script_ancestry = GetAncestry(*out_ad_script);
    }
  }

  return is_ad_script_in_stack;
}

void AdTracker::Shutdown() {
  ScriptAncestryTracker::Shutdown();
}

bool AdTracker::IsMarkedScript(V8ScriptId script_id) const {
  return ad_scripts_.Contains(script_id);
}

void AdTracker::OnScriptRegistered(ExecutionContext& execution_context,
                                   V8ScriptId script_id,
                                   const String& url,
                                   std::optional<V8ScriptId> marked_script_id) {
  std::optional<AdProvenance> ad_provenance;

  if (IsKnownAdExecutionContext(&execution_context)) {
    // It's an ad script because it's in an ad frame, but we don't (yet) specify
    // provenance for ad frames.
    ad_provenance = NoProvenance{};
  } else {
    // Check if this script itself was explicitly marked by the filter list
    // first.
    auto it = context_known_ad_scripts_.find(&execution_context);
    if (it != context_known_ad_scripts_.end()) {
      auto url_it = it->value.find(url);
      if (url_it != it->value.end()) {
        ad_provenance = url_it->value;
      }
    }
    // If not a direct match, check if the parent script was an ad (transitive).
    if (!ad_provenance.has_value() && marked_script_id.has_value()) {
      ad_provenance = *marked_script_id;
    }
  }

  if (ad_provenance.has_value()) {
    ad_scripts_.insert(script_id, *ad_provenance);
  }
}

void AdTracker::Trace(Visitor* visitor) const {
  ScriptAncestryTracker::Trace(visitor);
  visitor->Trace(context_known_ad_scripts_);
}

}  // namespace blink
