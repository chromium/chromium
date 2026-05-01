// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INJECTED_SCRIPT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INJECTED_SCRIPT_MANAGER_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace v8_inspector {
class V8InspectorSession;
}

namespace blink {

class InspectedFrames;
class LocalFrame;
class DOMWrapperWorld;

class CORE_EXPORT InspectorInjectedScriptManager final
    : public GarbageCollected<InspectorInjectedScriptManager> {
 public:
  InspectorInjectedScriptManager(InspectorAgentState* agent_state,
                                 InspectedFrames* inspected_frames,
                                 v8_inspector::V8InspectorSession* v8_session,
                                 InspectorPageAgent::Client* client);

  InspectorInjectedScriptManager(const InspectorInjectedScriptManager&) =
      delete;
  InspectorInjectedScriptManager& operator=(
      const InspectorInjectedScriptManager&) = delete;
  ~InspectorInjectedScriptManager() = default;

  void Trace(Visitor* visitor) const;

  String AddScriptToEvaluateOnNewDocument(
      const String& source,
      std::optional<String> world_name,
      std::optional<bool> include_command_line_api,
      std::optional<bool> runImmediately);

  bool RemoveScriptToEvaluateOnNewDocument(const String& identifier);

  void InjectScripts(LocalFrame* frame);

 private:
  void EvaluateScriptOnNewDocument(LocalFrame& frame,
                                   const String& script_identifier);

  DOMWrapperWorld* EnsureDOMWrapperWorld(LocalFrame* frame,
                                         const String& world_name,
                                         bool grant_universal_access);

  Member<InspectedFrames> inspected_frames_;
  v8_inspector::V8InspectorSession* const v8_session_;
  InspectorPageAgent::Client* const client_;

  InspectorAgentState::StringMap scripts_to_evaluate_on_load_;
  InspectorAgentState::StringMap worlds_to_evaluate_on_load_;
  InspectorAgentState::BooleanMap
      include_command_line_api_for_scripts_to_evaluate_on_load_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INJECTED_SCRIPT_MANAGER_H_
