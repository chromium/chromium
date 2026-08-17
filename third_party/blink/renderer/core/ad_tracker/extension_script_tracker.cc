// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/extension_script_tracker.h"

#include <optional>

#include "base/check.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/ad_tracker/script_ancestry_tracker.h"
#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

bool IsExtensionScriptUrl(const String& url) {
  if (url.empty()) {
    return false;
  }
  KURL kurl(url);
  return kurl.IsValid() &&
         CommonSchemeRegistry::IsExtensionScheme(kurl.Protocol().Ascii());
}
}  // namespace

ExtensionScriptTracker::ExtensionScriptTracker(LocalFrame* local_root,
                                               ScriptInitiationMonitor* monitor)
    : ScriptAncestryTracker(local_root, monitor) {
  CHECK(monitor);
}

ExtensionScriptTracker::~ExtensionScriptTracker() = default;

bool ExtensionScriptTracker::IsExtensionScriptInStack(
    StackType stack_type,
    MonkeyPatchableApi ignore_monkey_patch) {
  v8::Isolate* isolate = GetIsolate();
  if (!isolate) {
    return false;
  }
  LazyStackTrace stack_trace(isolate);
  return IsMarkedScriptInStack(stack_type, stack_trace, nullptr,
                               ignore_monkey_patch);
}

void ExtensionScriptTracker::Shutdown() {
  extension_scripts_.clear();
  extension_script_urls_.clear();
  ScriptAncestryTracker::Shutdown();
}

bool ExtensionScriptTracker::IsMarkedScript(V8ScriptId script_id) const {
  return extension_scripts_.Contains(script_id);
}

bool ExtensionScriptTracker::IsExtensionScriptUrlMarked(
    const String& url) const {
  CHECK(RuntimeEnabledFeatures::ExtensionScriptTaggingTestingAPIEnabled());
  if (url.empty()) {
    return false;
  }
  return extension_script_urls_.Contains(url);
}

void ExtensionScriptTracker::OnScriptRegistered(
    ExecutionContext& execution_context,
    V8ScriptId script_id,
    const String& url,
    std::optional<V8ScriptId> marked_script_id) {
  if (IsExtensionScriptUrl(url) ||
      GetScriptInitiationMonitor()->IsExecutingInjectedExtensionScript() ||
      marked_script_id.has_value()) {
    extension_scripts_.insert(script_id);
    if (!url.empty() &&
        RuntimeEnabledFeatures::ExtensionScriptTaggingTestingAPIEnabled()) {
      extension_script_urls_.insert(url);
    }
  }
}

void ExtensionScriptTracker::Trace(Visitor* visitor) const {
  ScriptAncestryTracker::Trace(visitor);
}

}  // namespace blink
