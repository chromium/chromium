// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_injected_script_manager.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8.h"

namespace blink {

InspectorInjectedScriptManager::InspectorInjectedScriptManager(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

void InspectorInjectedScriptManager::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
}

void InspectorInjectedScriptManager::SetV8Session(
    v8_inspector::V8InspectorSession* v8_session) {
  v8_session_ = v8_session;
}

void InspectorInjectedScriptManager::AddScriptToEvaluateOnNewDocument(
    const String& identifier,
    mojom::blink::ScriptToEvaluateOnNewDocumentPtr script,
    bool run_immediately) {
  scripts_.Set(identifier, std::move(script));

  if (run_immediately) {
    for (LocalFrame* frame : *inspected_frames_) {
      if (!frame->IsProvisional()) {
        EvaluateScriptOnNewDocument(*frame, identifier);
      }
    }
  }
}

bool InspectorInjectedScriptManager::RemoveScriptToEvaluateOnNewDocument(
    const String& identifier) {
  auto it = scripts_.find(identifier);
  if (it == scripts_.end()) {
    return false;
  }
  scripts_.erase(it);
  return true;
}

void InspectorInjectedScriptManager::InjectScripts(LocalFrame* frame) {
  Vector<String> keys;
  for (const auto& key : scripts_.Keys()) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end(), [](const String& a, const String& b) {
    return Decimal::FromString(a) < Decimal::FromString(b);
  });

  for (const String& key : keys) {
    EvaluateScriptOnNewDocument(*frame, key);
  }
}

void InspectorInjectedScriptManager::EvaluateScriptOnNewDocument(
    LocalFrame& frame,
    const String& script_identifier) {
  auto it = scripts_.find(script_identifier);
  if (it == scripts_.end()) {
    return;
  }
  const auto& script = it->value;

  auto* window = frame.DomWindow();
  v8::HandleScope handle_scope(window->GetIsolate());

  ScriptState* script_state = nullptr;
  const String world_name = script->world_name;
  if (world_name.empty()) {
    script_state = ToScriptStateForMainWorld(window->GetFrame());
  } else if (DOMWrapperWorld* world = EnsureDOMWrapperWorld(
                 &frame, world_name, true /* grant_universal_access */)) {
    script_state =
        ToScriptState(window->GetFrame(),
                      *DOMWrapperWorld::EnsureIsolatedWorld(
                          ToIsolate(window->GetFrame()), world->GetWorldId()));
  }
  if (!script_state || !v8_session_) {
    return;
  }

  v8_session_->evaluate(script_state->GetContext(),
                        ToV8InspectorStringView(script->source),
                        script->include_command_line_api);
  // Note v8_session_ may be null here as the session may have been disposed
  // during the execution of the injected script.
}

DOMWrapperWorld* InspectorInjectedScriptManager::EnsureDOMWrapperWorld(
    LocalFrame* frame,
    const String& world_name,
    bool grant_universal_access) {
  LocalDOMWindow* window = frame->DomWindow();
  DOMWrapperWorld* world =
      DOMWrapperWorld::EnsureInspectorIsolatedWorldWithName(
          frame->DomWindow()->GetIsolate(), world_name);
  if (!world) {
    return nullptr;
  }
  scoped_refptr<SecurityOrigin> security_origin =
      window->GetSecurityOrigin()->IsolatedCopy();
  if (grant_universal_access) {
    security_origin->GrantUniversalAccess();
  }
  DOMWrapperWorld::SetIsolatedWorldSecurityOrigin(world->GetWorldId(),
                                                  security_origin);
  return world;
}

}  // namespace blink
