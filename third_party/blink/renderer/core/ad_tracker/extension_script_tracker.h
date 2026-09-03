// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_EXTENSION_SCRIPT_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_EXTENSION_SCRIPT_TRACKER_H_

#include <optional>

#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class ScriptInitiationMonitor;

// Tracker for tagging scripts injected into web pages by chrome extensions.
// The tracker is maintained per local root.
class CORE_EXPORT ExtensionScriptTracker : public ScriptAncestryTracker {
 public:
  ExtensionScriptTracker(LocalFrame*, ScriptInitiationMonitor*);
  ExtensionScriptTracker(const ExtensionScriptTracker&) = delete;
  ExtensionScriptTracker& operator=(const ExtensionScriptTracker&) = delete;
  ~ExtensionScriptTracker() override;

  // If an extension script is in the call stack, returns the ID of the
  // extension that injected or initiated the script, or an empty String if no
  // extension script was found.
  String ExtensionScriptInStack(
      StackType stack_type = StackType::kTopOnly,
      MonkeyPatchableApi ignore_monkey_patch = MonkeyPatchableApi::kNone);

  // ScriptAncestryTracker overrides:
  ScriptAncestryTrackerType GetTrackerType() const override {
    return ScriptAncestryTrackerType::kExtension;
  }
  void Shutdown() override;
  bool IsMarkedScript(V8ScriptId) const override;
  void OnScriptRegistered(ExecutionContext& execution_context,
                          V8ScriptId script_id,
                          const String& url,
                          std::optional<V8ScriptId> marked_script_id) override;
  void Trace(Visitor*) const override;

  // Returns true if the script URL was marked as an extension script during
  // execution. Note: URL tracking is test-only and enabled via
  // ExtensionScriptTaggingTestingAPI.
  bool IsExtensionScriptUrlMarked(const String& url) const;

 private:
  friend class ExtensionScriptTrackerTest;

  String GetExtensionIdForScript(std::optional<V8ScriptId> script_id) const;

  HashMap<V8ScriptId, String> extension_scripts_;
  // Test-only set of extension script URLs, populated when
  // ExtensionScriptTaggingTestingAPI is enabled.
  HashSet<String> extension_script_urls_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_EXTENSION_SCRIPT_TRACKER_H_
