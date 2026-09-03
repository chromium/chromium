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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

String GetExtensionIdFromUrl(const String& url) {
  if (url.empty()) {
    return String();
  }
  KURL kurl(url);
  if (!kurl.IsValid() ||
      !CommonSchemeRegistry::IsExtensionScheme(kurl.Protocol().Ascii())) {
    return String();
  }
  return kurl.Host().ToString();
}

String GetExtensionIdForCurrentWorld(ExecutionContext& execution_context) {
  const DOMWrapperWorld* world = execution_context.GetCurrentWorld();
  if (!world || !world->IsIsolatedWorld()) {
    return String();
  }
  scoped_refptr<const SecurityOrigin> origin =
      world->IsolatedWorldSecurityOrigin(execution_context.GetAgentClusterID());
  if (origin &&
      CommonSchemeRegistry::IsExtensionScheme(origin->Protocol().Ascii())) {
    return origin->Host();
  }
  return String();
}
}  // namespace

ExtensionScriptTracker::ExtensionScriptTracker(LocalFrame* local_root,
                                               ScriptInitiationMonitor* monitor)
    : ScriptAncestryTracker(local_root, monitor) {
  CHECK(monitor);
}

ExtensionScriptTracker::~ExtensionScriptTracker() = default;

String ExtensionScriptTracker::ExtensionScriptInStack(
    StackType stack_type,
    MonkeyPatchableApi ignore_monkey_patch) {
  v8::Isolate* isolate = GetIsolate();
  if (!isolate) {
    return String();
  }
  LazyStackTrace stack_trace(isolate);
  std::optional<V8ScriptId> marked_script =
      GetMarkedScriptInStack(stack_type, stack_trace, ignore_monkey_patch);
  return GetExtensionIdForScript(marked_script);
}

String ExtensionScriptTracker::GetExtensionIdForScript(
    std::optional<V8ScriptId> script_id) const {
  if (!script_id.has_value() || script_id->value() <= 0) {
    return String();
  }
  auto it = extension_scripts_.find(*script_id);
  return (it != extension_scripts_.end()) ? it->value : String();
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

// A script has been registered. Determine if it's extension related, and if so,
// which extension to attribute its load to.
void ExtensionScriptTracker::OnScriptRegistered(
    ExecutionContext& execution_context,
    V8ScriptId script_id,
    const String& url,
    std::optional<V8ScriptId> marked_script_id) {
  // 1. Inherit from the calling/initiating script in the active call stack.
  // This takes precedence to attribute causality to the caller (e.g. if an
  // extension injects a web script or loads another extension's URL).
  String extension_id = GetExtensionIdForScript(marked_script_id);

  // 2. Attributed to the currently executing injected script scope.
  if (extension_id.empty()) {
    extension_id = GetScriptInitiationMonitor()->CurrentScriptInjectorId();
  }

  // 3. Isolated world security origin of the execution context running the
  // script (e.g. content scripts executing within their own isolated world).
  if (extension_id.empty()) {
    extension_id = GetExtensionIdForCurrentWorld(execution_context);
  }

  // 4. Explicit extension scheme URL (e.g. web-accessible resources loaded by
  // the page).
  if (extension_id.empty()) {
    extension_id = GetExtensionIdFromUrl(url);
  }

  // 5. Inherited from the initiating script of the frame if running inside an
  // extension-created subframe.
  if (extension_id.empty()) {
    if (auto* window = DynamicTo<LocalDOMWindow>(&execution_context)) {
      if (LocalFrame* frame = window->GetFrame(); IsMarkedFrame(frame)) {
        extension_id = GetExtensionIdForScript(GetInitiatingScriptId(frame));
      }
    }
  }

  if (!extension_id.empty()) {
    extension_scripts_.insert(script_id, extension_id);
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
