// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_

#include <stdint.h>

#include <optional>
#include <variant>

#include "components/subresource_filter/core/common/scoped_rule.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/ad_script_identifier.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalFrame;
enum class ResourceType : uint8_t;

namespace probe {
class AsyncTaskContext;
class CallFunction;
class ExecuteScript;
}  // namespace probe

// Tracker for tagging resources as ads based on the call stack scripts.
// The tracker is maintained per local root.
class CORE_EXPORT AdTracker : public GarbageCollected<AdTracker> {
 public:
  // A list of JavaScript APIs that are frequently monkey patched by ad scripts.
  // This enum is used as a parameter to `IsAdScriptInStack` to enable a
  // heuristic that can ignore a top-level ad script, preventing false positives
  // when the API is called from a non-ad script through an ad script's monkey
  // patch.
  enum class MonkeyPatchableApi {
    // Default setting to disable the heuristic.
    kNone,

    // history.pushState
    kHistoryPushState,
  };

  struct NoProvenance {};

  // Represents the reason why a script is classified as an ad. It can be one
  // of:
  // - NoProvenance: The script has neither an ancestor nor a rule match.
  // - subresource_filter::ScopedRule: The script is flagged by the subresource
  //   filter.
  // - script_id: The script itself is not flagged, but another ad
  //   script (the "ancestor") exists in its creation stack.
  using AdProvenance =
      std::variant<NoProvenance, subresource_filter::ScopedRule, int>;

  enum class StackType { kBottomOnly, kBottomAndTop };

  struct AdScriptAncestry {
    // A chain of `AdScriptIdentifier`s representing the ancestry of an ad
    // script. The chain is ordered from the script itself (lower level) up to
    // its root ancestor that was flagged by filterlist.
    Vector<AdScriptIdentifier> ancestry_chain;

    // The filterlist rule that caused the root (last) script in
    // `ancestry_chain` to be ad-tagged.
    subresource_filter::ScopedRule root_script_filterlist_rule;

    // A brief summary of the ancestry. Useful for intervention reports.
    String ToString() const;
  };

  // Finds an AdTracker for a given ExecutionContext.
  static AdTracker* FromExecutionContext(ExecutionContext*);

  static bool IsAdScriptExecutingInDocument(
      Document* document,
      StackType stack_type = StackType::kBottomAndTop);

  // Instrumenting methods.
  // Called when a script module or script gets executed from native code.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  // Called when a function gets called from native code.
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  // Called when a subresource request is about to be sent or is redirected.
  // Returns true if any of the following are true:
  // - the resource is loaded in an ad iframe
  // - `known_ad` is true
  // - ad script is in the v8 stack and the resource was not requested by CSS.
  // This check is only done if `scan_stack_for_ads` is true.
  // Virtual for testing.
  virtual bool CalculateIfAdSubresource(
      ExecutionContext* execution_context,
      const KURL& request_url,
      ResourceType resource_type,
      const FetchInitiatorInfo& initiator_info,
      bool known_ad,
      bool scan_stack_for_ads,
      const subresource_filter::ScopedRule& rule);

  // Called when an async task is created. Check at this point for ad script on
  // the stack and annotate the task if so.
  void DidCreateAsyncTask(probe::AsyncTaskContext* task_context);

  // Called when an ad-related async task is eventually run.
  void DidStartAsyncTask(probe::AsyncTaskContext* task_context);

  // Called when the ad-related task has finished running.
  void DidFinishAsyncTask(probe::AsyncTaskContext* task_context);

  // Returns true if any script in the pseudo call stack has previously been
  // identified as an ad resource, if the current ExecutionContext is a known ad
  // execution context, or if the script at the top of isolate's
  // stack is ad script. Whether to look at just the bottom of the
  // stack or the top and bottom is indicated by `stack_type`. kBottomAndTop is
  // generally best as it catches more ads, but if you're calling very
  // frequently then consider just the bottom of the stack for performance sake.
  //
  // When `ignore_monkey_patch` is specified, a heuristic is enabled to prevent
  // false positives from monkey patching. If the script at the top of the stack
  // is an ad script and the API was invoked by non-ad script, this check will
  // be ignored for the first call to the specified API within the current
  // synchronous task. This is because the ad script is likely just a proxy for
  // the real, non-ad caller.
  //
  // Note: This function is not idempotent when `ignore_monkey_patch` is used,
  // as it tracks the first call to an API within a synchronous task.
  //
  // Output Parameters:
  // - `out_ad_script_ancestry`: if non-null and there is ad script in the
  //   stack, this will be populated with the ad script's ancestry and the
  //   triggering filterlist rule. See `AdScriptAncestry` for more details on
  //   the populated fields.
  virtual bool IsAdScriptInStack(
      StackType stack_type,
      MonkeyPatchableApi ignore_monkey_patch = MonkeyPatchableApi::kNone,
      AdScriptAncestry* out_ad_script_ancestry = nullptr);

  virtual void Trace(Visitor*) const;

  void Shutdown();
  explicit AdTracker(LocalFrame*);
  AdTracker(const AdTracker&) = delete;
  AdTracker& operator=(const AdTracker&) = delete;
  virtual ~AdTracker();

 private:
  friend class FrameFetchContextSubresourceFilterTest;
  friend class AdTrackerSimTest;
  friend class AdTrackerTest;

  struct AdScriptData {
    AdScriptIdentifier id;
    AdProvenance provenance;
  };

  ExecutionContext* GetCurrentExecutionContext(v8::Isolate*);

  // Similar to the public IsAdScriptInStack method but instead of returning an
  // ancestry chain, it returns only one script (the most immediate one).
  bool IsAdScriptInStackHelper(
      StackType stack_type,
      MonkeyPatchableApi ignore_monkey_patch,
      std::optional<AdScriptIdentifier>* out_ad_script);

  // Helper for the `ignore_monkey_patch` heuristic. Returns true if the API is
  // called from a non-ad script through an ad script's monkey patch, and this
  // is the first time this API has been called this way within the current
  // synchronous task. If it returns true, the call should be ignored for ad
  // tracking purposes. This method is not const because it modifies
  // `ad_monkey_patch_calls_in_scope_`.
  //
  // Precondition: The script at the top of the stack is a known ad script.
  bool IsFirstCallOfApiFromNonAdScript(v8::Isolate* isolate,
                                       MonkeyPatchableApi api);

  // Helper for `IsFirstCallOfApiFromNonAdScript` that performs the stack
  // analysis. It returns true if the call stack indicates that a non-ad script
  // called the monkey patched `api`.
  bool WasApiCalledByNonAdScript(v8::Isolate* isolate,
                                 MonkeyPatchableApi api) const;

  bool IsKnownAdScript(ExecutionContext*, const String& url);

  // Adds the given `url` and its associated `ad_provenance` to the set of known
  // ad scripts associated with the provided `execution_context`.
  void AppendToKnownAdScripts(ExecutionContext& execution_context,
                              const String& url,
                              AdProvenance ad_provenance);

  // Handles the discovery of a script ID for a known ad script. It creates and
  // links a new AdScriptIdentifier (with `script_id` and `v8_context`) to the
  // provenance of `script_name`. The new link is kept in `script_provenances_`.
  //
  // Prerequisites: `script_name` is a known ad script in `execution_context`.
  void OnScriptIdAvailableForKnownAdScript(
      ExecutionContext* execution_context,
      const v8::Local<v8::Context>& v8_context,
      const String& script_name,
      int script_id);

  // Retrieves the ancestry chain of a given ad script (inclusive) and and the
  // triggering filterlist rule. See `AdScriptAncestry` for more details on the
  // populated fields.
  AdScriptAncestry GetAncestry(const AdScriptIdentifier& ad_script);

  Member<LocalFrame> local_root_;

  // Indicates the bottom-most synchronous ad script on the stack or
  // `std::nullopt` if there isn't one.
  std::optional<int> bottom_most_ad_script_;

  // Indicates the bottom-most ad script on the async stack or `std::nullopt`
  // if there isn't one.
  std::optional<AdScriptIdentifier> bottom_most_async_ad_script_;

  // Maps the URL of a detected ad script to its AdProvenance.
  //
  // Script Identification:
  // - Scripts with a resource URL are identified by that URL.
  // - Inline scripts (without a URL) are assigned a unique synthetic URL
  //   generated by `GenerateFakeUrlFromScriptId()`.
  using KnownAdScriptsAndProvenance = HashMap<String, AdProvenance>;

  // Tracks ad scripts detected outside of ad-frame contexts.
  HeapHashMap<WeakMember<ExecutionContext>, KnownAdScriptsAndProvenance>
      context_known_ad_scripts_;

  // A map of all known ad script ids to their metadata.
  HashMap<int, AdScriptData> ad_script_data_;

  // Tracks APIs that have been identified as being called through an ad
  // script's monkey patch within the current synchronous task. This set is
  // cleared when the task completes. Used by the `ignore_monkey_patch`
  // heuristic.
  HashSet<MonkeyPatchableApi> ad_monkey_patch_calls_in_scope_;

  // The number of ad-related async tasks currently running in the stack.
  int running_ad_async_tasks_ = 0;

  // The number of sync tasks currently running in the stack.
  int running_sync_tasks_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
