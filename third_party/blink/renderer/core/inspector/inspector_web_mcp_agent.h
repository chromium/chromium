// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/web_mcp.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace blink {

class Document;
class InspectedFrames;
class LocalFrame;
class ModelContext;
class ScriptState;
struct ScriptToolError;
class ToolData;

class CORE_EXPORT InspectorWebMCPAgent final
    : public InspectorBaseAgent<protocol::WebMCP::Metainfo> {
 public:
  explicit InspectorWebMCPAgent(InspectedFrames* inspected_frames,
                                v8_inspector::V8InspectorSession* v8_session);
  InspectorWebMCPAgent(const InspectorWebMCPAgent&) = delete;
  InspectorWebMCPAgent& operator=(const InspectorWebMCPAgent&) = delete;
  ~InspectorWebMCPAgent() override;

  void Trace(Visitor*) const override;

  // InspectorBaseAgent overrides.
  void Restore() override;

  // Protocol methods.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response invokeTool(
      const String& frameId,
      const String& toolName,
      std::unique_ptr<protocol::DictionaryValue> input,
      String* executionId) override;

  // Probes
  void WebMCPToolAdded(Document* document, const ToolData& name);
  void WebMCPToolRemoved(Document* document, const ToolData& name);
  void WebMCPToolExecuted(Document* document,
                          const String& name,
                          const String& input_arguments,
                          const base::UnguessableToken& execution_id);
  void WebMCPToolResponded(Document* document,
                           const String& result,
                           const base::UnguessableToken& execution_id);
  void WebMCPToolFailed(
      Document* document,
      const ScriptToolError& error,
      const base::UnguessableToken& execution_id,
      std::optional<std::pair<ScriptValue, ScriptState*>> exception);

 private:
  Member<InspectedFrames> inspected_frames_;
  v8_inspector::V8InspectorSession* v8_session_;
  InspectorAgentState::Boolean enabled_;

  ModelContext* GetModelContext(LocalFrame* frame);

  std::unique_ptr<protocol::WebMCP::Tool> BuildProtocolTool(LocalFrame* frame,
                                                            const ToolData&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_
