// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/web_mcp.h"

namespace blink {

class InspectedFrames;
class LocalFrame;
class ModelContext;
class Document;
class ToolData;

class CORE_EXPORT InspectorWebMCPAgent final
    : public InspectorBaseAgent<protocol::WebMCP::Metainfo> {
 public:
  explicit InspectorWebMCPAgent(InspectedFrames* inspected_frames);
  InspectorWebMCPAgent(const InspectorWebMCPAgent&) = delete;
  InspectorWebMCPAgent& operator=(const InspectorWebMCPAgent&) = delete;
  ~InspectorWebMCPAgent() override;

  void Trace(Visitor*) const override;

  // InspectorBaseAgent overrides.
  void Restore() override;

  // Protocol methods.
  protocol::Response enable() override;

  // Probes
  void WebMCPToolAdded(Document* document, const ToolData& name);
  void WebMCPToolRemoved(Document* document, const ToolData& name);

 private:
  Member<InspectedFrames> inspected_frames_;
  InspectorAgentState::Boolean enabled_;

  ModelContext* GetModelContext(LocalFrame* frame);
  std::unique_ptr<protocol::WebMCP::Tool> BuildProtocolTool(LocalFrame* frame,
                                                            const ToolData&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_WEB_MCP_AGENT_H_
