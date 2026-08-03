// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_AD_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_AD_TRACKER_H_

#include <stdint.h>

#include <optional>

#include "base/gtest_prod_util.h"
#include "components/subresource_filter/core/common/scoped_rule.h"
#include "third_party/blink/renderer/core/ad_tracker/ad_script_identifier.h"
#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalFrame;
class ScriptInitiationMonitor;
enum class ResourceType : uint8_t;

// Tracker for tagging resources as ads based on the call stack scripts.
// The tracker is maintained per local root.
class CORE_EXPORT AdTracker : public ScriptAncestryTracker {
 public:
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
      StackType stack_type = StackType::kTopOnly);

  AdTracker(LocalFrame*, ScriptInitiationMonitor*);
  AdTracker(const AdTracker&) = delete;
  AdTracker& operator=(const AdTracker&) = delete;
  ~AdTracker() override;

  // Returns whether the given subresource request is on behalf of advertising.
  virtual std::optional<AdProvenance> CalculateIfAdSubresource(
      ExecutionContext* execution_context,
      const KURL& request_url,
      ResourceType resource_type,
      const FetchInitiatorInfo& initiator_info,
      std::optional<AdProvenance> known_ad_provenance,
      bool scan_javascript_stack);

  bool IsKnownAdScript(ExecutionContext*, const String& url);

  // Retrieves the ancestry chain of a given ad script (inclusive) and the
  // triggering filterlist rule. See `AdScriptAncestry` for more details on the
  // populated fields.
  AdScriptAncestry GetAncestry(V8ScriptId script_id);

  // Returns true if any script in the pseudo call stack has previously been
  // identified as an ad resource, if the current ExecutionContext is a known ad
  // execution context, or if the script at the top of isolate's
  // stack is ad script. Whether to look at just the bottom of the
  // stack or the top and bottom is indicated by `stack_type`. `kTopOnly` is
  // generally best as it catches more ads but you may want to call
  // `kBottomOnly` if you truly only care about that frame.
  //
  // When `ignore_monkey_patch` is specified, a heuristic is enabled to mitigate
  // inaccurate stack tagging caused by API monkey patching (i.e., the immediate
  // caller is a proxy for the real caller). This handles two distinct
  // scenarios:
  // 1. Ad Monkey Patch (Mitigating False Positives):
  //    If the script at the top of the stack is an ad script and the API was
  //    invoked by a non-ad script, this check will be ignored for the first
  //    call to the specified API within the current synchronous task.
  // 2. Non-Ad Monkey Patch (Mitigating False Negatives):
  //    If the script at the top of the stack is a non-ad script and the API was
  //    invoked by an ad script, the stack is classified as ad-related.
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

  // ScriptAncestryTracker overrides:
  void Shutdown() override;
  bool IsMarkedScript(V8ScriptId) const override;
  void OnScriptRegistered(ExecutionContext& execution_context,
                          V8ScriptId script_id,
                          const String& url,
                          std::optional<V8ScriptId> marked_script_id) override;
  void Trace(Visitor*) const override;

 private:
  friend class FrameFetchContextSubresourceFilterTest;
  friend class AdTrackerSimTest;
  friend class AdTrackerTest;
  FRIEND_TEST_ALL_PREFIXES(AdTrackerTest,
                           AdScriptAncestry_ScriptIdFromDifferentTracker);

  using KnownAdScriptsAndProvenance = HashMap<String, AdProvenance>;
  HeapHashMap<WeakMember<ExecutionContext>, KnownAdScriptsAndProvenance>
      context_known_ad_scripts_;
  HashMap<V8ScriptId, AdProvenance> ad_scripts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_AD_TRACKER_H_
