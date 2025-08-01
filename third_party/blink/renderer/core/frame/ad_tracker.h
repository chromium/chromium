// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_

#include <stdint.h>

#include <optional>

#include "base/feature_list.h"
#include "components/subresource_filter/core/common/scoped_rule.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/ad_script_identifier.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
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
  struct AdProvenance {
    // Represents the reason why a script is classified as an ad.
    enum class ProvenanceType {
      // The script is flagged by the subresource filter.
      kMatchedRule,

      // The script itself is not flagged by the subresource filter, but another
      // ad script (i.e., the "ancestor") exists in its creation stack.
      kAncestorScript,

      // The ad script has neither an ancestor nor a rule match. This can happen
      // if:
      // 1) A non-filterlisted URL, initially a redirect target from a
      //    filterlisted URL, is later encountered again when loading this
      //    script.
      // 2) The script originates from an ad context without further traceable
      //    script.
      //
      // TODO(yaoxia): Re-evaluate the necessity of this type once
      // crbug.com/417756984 and crbug.com/421202278 are fixed.
      kNone,
    };

    virtual ~AdProvenance() = default;

    virtual std::unique_ptr<AdProvenance> Clone() const = 0;

    virtual ProvenanceType Type() const = 0;
  };

  struct AdRulesetProvenance : public AdProvenance {
    AdRulesetProvenance(const subresource_filter::ScopedRule& filterlist_rule)
        : filterlist_rule(filterlist_rule) {}

    std::unique_ptr<AdProvenance> Clone() const override {
      return std::make_unique<AdRulesetProvenance>(*this);
    }

    ProvenanceType Type() const override {
      return ProvenanceType::kMatchedRule;
    }

    // The filterlist rule that caused this script to be flagged as an ad.
    subresource_filter::ScopedRule filterlist_rule;
  };

  struct AdAncestorProvenance : public AdProvenance {
    AdAncestorProvenance(const AdScriptIdentifier& ancestor_ad_script)
        : ancestor_ad_script(ancestor_ad_script) {}

    std::unique_ptr<AdProvenance> Clone() const override {
      return std::make_unique<AdAncestorProvenance>(*this);
    }

    ProvenanceType Type() const override {
      return ProvenanceType::kAncestorScript;
    }

    // This script's ancestor ad script in the creation stack.
    AdScriptIdentifier ancestor_ad_script;
  };

  struct NoAdProvenance : public AdProvenance {
    std::unique_ptr<AdProvenance> Clone() const override {
      return std::make_unique<NoAdProvenance>(*this);
    }

    ProvenanceType Type() const override { return ProvenanceType::kNone; }
  };

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
  // - |known_ad| is true
  // - ad script is in the v8 stack and the resource was not requested by CSS.
  // Virtual for testing.
  virtual bool CalculateIfAdSubresource(
      ExecutionContext* execution_context,
      const KURL& request_url,
      ResourceType resource_type,
      const FetchInitiatorInfo& initiator_info,
      bool known_ad,
      const subresource_filter::ScopedRule& rule);

  // Called when an async task is created. Check at this point for ad script on
  // the stack and annotate the task if so.
  void DidCreateAsyncTask(probe::AsyncTaskContext* task_context);

  // Called when an async task is eventually run.
  void DidStartAsyncTask(probe::AsyncTaskContext* task_context);

  // Called when the task has finished running.
  void DidFinishAsyncTask(probe::AsyncTaskContext* task_context);

  // Returns true if any script in the pseudo call stack has previously been
  // identified as an ad resource, if the current ExecutionContext is a known ad
  // execution context, or if the script at the top of isolate's
  // stack is ad script. Whether to look at just the bottom of the
  // stack or the top and bottom is indicated by `stack_type`. kBottomAndTop is
  // generally best as it catches more ads, but if you're calling very
  // frequently then consider just the bottom of the stack for performance sake.
  //
  // Output Parameters:
  // - `out_ad_script_ancestry`: if non-null and there is ad script in the
  //   stack, this will be populated with the ad script's ancestry and the
  //   triggering filterlist rule. See `AdScriptAncestry` for more details on
  //   the populated fields.
  virtual bool IsAdScriptInStack(
      StackType stack_type,
      AdScriptAncestry* out_ad_script_ancestry = nullptr);

  virtual void Trace(Visitor*) const;

  void Shutdown();
  explicit AdTracker(LocalFrame*);
  AdTracker(const AdTracker&) = delete;
  AdTracker& operator=(const AdTracker&) = delete;
  virtual ~AdTracker();

 protected:
  // Protected for testing.
  // Note that this outputs the `out_top_script` even when it's not an ad.
  virtual int ScriptAtTopOfStack();
  virtual ExecutionContext* GetCurrentExecutionContext();

  // `script_name` will be empty in the case of a dynamically added script with
  // no src attribute set. `script_id` won't be set for module scripts in an
  // errored state or for non-source text modules. `top_level_execution` should
  // be true if the top-level script is being run, as opposed to a function
  // being called.
  virtual void WillExecuteScript(ExecutionContext*,
                                 const v8::Local<v8::Context>& v8_context,
                                 const String& script_name,
                                 int script_id,
                                 bool top_level_execution);

 private:
  friend class FrameFetchContextSubresourceFilterTest;
  friend class AdTrackerSimTest;
  friend class AdTrackerTest;

  // Similar to the public IsAdScriptInStack method but instead of returning an
  // ancestry chain, it returns only one script (the most immediate one).
  bool IsAdScriptInStackHelper(
      StackType stack_type,
      std::optional<AdScriptIdentifier>* out_ad_script);

  void DidExecuteScript();
  bool IsKnownAdScript(ExecutionContext*, const String& url);

  // Adds the given `url` and its associated `ad_provenance` to the set of known
  // ad scripts associated with the provided `execution_context`.
  void AppendToKnownAdScripts(ExecutionContext& execution_context,
                              const String& url,
                              std::unique_ptr<AdProvenance> ad_provenance);

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

  // Each time v8 is started to run a script or function, this records if it was
  // an ad script. Each time the script or function finishes, it pops the stack.
  Vector<bool> stack_frame_is_ad_;

  int num_ads_in_stack_ = 0;

  // Indicates the bottom-most ad script on the stack or `std::nullopt` if
  // there isn't one. A non-null value implies `num_ads_in_stack > 0`.
  std::optional<AdScriptIdentifier> bottom_most_ad_script_;

  // Indicates the bottom-most ad script on the async stack or `std::nullopt`
  // if there isn't one.
  std::optional<AdScriptIdentifier> bottom_most_async_ad_script_;

  // Maps the URL of a detected ad script to its AdProvenance.
  //
  // Script Identification:
  // - Scripts with a resource URL are identified by that URL.
  // - Inline scripts (without a URL) are assigned a unique synthetic URL
  //   generated by `GenerateFakeUrlFromScriptId()`.
  using KnownAdScriptsAndProvenance =
      HashMap<String, std::unique_ptr<AdProvenance>>;

  // Tracks ad scripts detected outside of ad-frame contexts.
  HeapHashMap<WeakMember<ExecutionContext>, KnownAdScriptsAndProvenance>
      context_known_ad_scripts_;

  // Maps the identifier of a detected ad script to its AdProvenance.
  HashMap<AdScriptIdentifier, std::unique_ptr<AdProvenance>>
      ad_script_provenances_;

  // The number of ad-related async tasks currently running in the stack.
  int running_ad_async_tasks_ = 0;

  // The known ad-related script ids.
  HashSet<int> ad_script_ids_;
};

template <>
struct DowncastTraits<AdTracker::AdRulesetProvenance> {
  static bool AllowFrom(const AdTracker::AdProvenance& ad_provenance) {
    return ad_provenance.Type() ==
           AdTracker::AdProvenance::ProvenanceType::kMatchedRule;
  }
};

template <>
struct DowncastTraits<AdTracker::AdAncestorProvenance> {
  static bool AllowFrom(const AdTracker::AdProvenance& ad_provenance) {
    return ad_provenance.Type() ==
           AdTracker::AdProvenance::ProvenanceType::kAncestorScript;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_TRACKER_H_
