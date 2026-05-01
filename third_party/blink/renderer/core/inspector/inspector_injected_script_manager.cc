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
    InspectorAgentState* agent_state,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session,
    InspectorPageAgent::Client* client)
    : inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      client_(client),
      scripts_to_evaluate_on_load_(agent_state, String()),
      worlds_to_evaluate_on_load_(agent_state, String()),
      include_command_line_api_for_scripts_to_evaluate_on_load_(agent_state,
                                                                false) {}

void InspectorInjectedScriptManager::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
}

String InspectorInjectedScriptManager::AddScriptToEvaluateOnNewDocument(
    const String& source,
    std::optional<String> world_name,
    std::optional<bool> include_command_line_api,
    std::optional<bool> runImmediately) {
  String identifier;
  {
    const auto& keys = scripts_to_evaluate_on_load_.Keys();
    auto result = std::max_element(
        keys.begin(), keys.end(), [](const String& a, const String& b) {
          return Decimal::FromString(a) < Decimal::FromString(b);
        });
    if (result == keys.end()) {
      identifier = String::Number(1);
    } else {
      identifier = String::Number(Decimal::FromString(*result).ToDouble() + 1);
    }
  }

  scripts_to_evaluate_on_load_.Set(identifier, source);
  worlds_to_evaluate_on_load_.Set(identifier, world_name.value_or(""));
  include_command_line_api_for_scripts_to_evaluate_on_load_.Set(
      identifier, include_command_line_api.value_or(false));

  if (client_->IsPausedForNewWindow() || runImmediately.value_or(false)) {
    // client_->IsPausedForNewWindow(): When opening a new popup,
    // Page.addScriptToEvaluateOnNewDocument could be called after
    // Runtime.enable that forces main context creation. In this case, we would
    // not normally evaluate the script, but we should.
    for (LocalFrame* frame : *inspected_frames_) {
      // Don't evaluate scripts on provisional frames:
      // https://crbug.com/390710982
      if (!frame->IsProvisional()) {
        EvaluateScriptOnNewDocument(*frame, identifier);
      }
    }
  }

  return identifier;
}

bool InspectorInjectedScriptManager::RemoveScriptToEvaluateOnNewDocument(
    const String& identifier) {
  if (scripts_to_evaluate_on_load_.Get(identifier).IsNull()) {
    return false;
  }
  scripts_to_evaluate_on_load_.Clear(identifier);
  worlds_to_evaluate_on_load_.Clear(identifier);
  include_command_line_api_for_scripts_to_evaluate_on_load_.Clear(identifier);
  return true;
}

void InspectorInjectedScriptManager::InjectScripts(LocalFrame* frame) {
  Vector<String> keys(scripts_to_evaluate_on_load_.Keys());
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
  auto* window = frame.DomWindow();
  v8::HandleScope handle_scope(window->GetIsolate());

  ScriptState* script_state = nullptr;
  const String world_name = worlds_to_evaluate_on_load_.Get(script_identifier);
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

  v8_session_->evaluate(
      script_state->GetContext(),
      ToV8InspectorStringView(
          scripts_to_evaluate_on_load_.Get(script_identifier)),
      include_command_line_api_for_scripts_to_evaluate_on_load_.Get(
          script_identifier));
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
