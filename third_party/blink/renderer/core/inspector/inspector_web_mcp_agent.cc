// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/inspector/inspector_web_mcp_agent.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {
InspectorWebMCPAgent::InspectorWebMCPAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      enabled_(&agent_state_, /*default_value=*/false) {}
InspectorWebMCPAgent::~InspectorWebMCPAgent() = default;

void InspectorWebMCPAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

void blink::InspectorWebMCPAgent::Restore() {
  if (enabled_.Get()) {
    enable();
  }
}

ModelContext* InspectorWebMCPAgent::GetModelContext(LocalFrame* frame) {
  if (!frame) {
    return nullptr;
  }
  auto* window = frame->DomWindow();
  if (!window) {
    return nullptr;
  }
  auto* navigator = window->navigator();
  if (!navigator) {
    return nullptr;
  }
  return ModelContextSupplement::GetIfExists(*navigator);
}

namespace {
std::unique_ptr<protocol::DictionaryValue> ParseJSON(const String& value) {
  std::vector<uint8_t> cbor;

  if (value.Is8Bit()) {
    crdtp::json::ConvertJSONToCBOR(crdtp::span<uint8_t>(value.Span8()), &cbor);
  } else {
    crdtp::json::ConvertJSONToCBOR(crdtp::span<uint16_t>(value.SpanUint16()),
                                   &cbor);
  }
  auto parsed_schema = protocol::DictionaryValue::cast(
      protocol::DictionaryValue::parseBinary(cbor.data(), cbor.size()));
  if (parsed_schema) {
    return parsed_schema;
  }
  return protocol::DictionaryValue::create();
}
}  // namespace

std::unique_ptr<protocol::WebMCP::Tool> InspectorWebMCPAgent::BuildProtocolTool(
    LocalFrame* frame,
    const ToolData& tool_data) {
  DCHECK(frame);

  auto tool = protocol::WebMCP::Tool::create()
                  .setName(tool_data.Name())
                  .setDescription(tool_data.ScriptTool().description)
                  .setFrameId(IdentifiersFactory::FrameId(frame))
                  .build();
  if (auto input_schema = tool_data.ScriptTool().input_schema) {
    tool->setInputSchema(ParseJSON(input_schema));
  }
  if (auto* node = tool_data.BackingFormElement()) {
    tool->setBackendNodeId(IdentifiersFactory::IntIdForNode(node));
  }
  if (auto* source_location = tool_data.GetSourceLocation()) {
    tool->setStackTrace(source_location->BuildInspectorObject());
  }
  return tool;
}

protocol::Response InspectorWebMCPAgent::enable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorWebMCPAgent(this);

  auto tools = std::make_unique<protocol::Array<protocol::WebMCP::Tool>>();
  if (auto* frame = inspected_frames_->Root()) {
    if (auto* model_context = GetModelContext(frame)) {
      for (auto tool : model_context->ListTools()) {
        tools->push_back(BuildProtocolTool(frame, *tool));
      }
    }
  }
  if (!tools->empty()) {
    GetFrontend()->toolsAdded(std::move(tools));
  }
  return protocol::Response::Success();
}

void InspectorWebMCPAgent::WebMCPToolAdded(Document* document,
                                           const ToolData& tool) {
  LocalFrame* frame = document->GetFrame();
  if (!enabled_.Get() || !frame) {
    return;
  }
  auto tools = std::make_unique<protocol::Array<protocol::WebMCP::Tool>>();
  tools->push_back(BuildProtocolTool(frame, tool));
  GetFrontend()->toolsAdded(std::move(tools));
}

void InspectorWebMCPAgent::WebMCPToolRemoved(Document* document,
                                             const ToolData& tool) {
  LocalFrame* frame = document->GetFrame();
  if (!enabled_.Get() || !frame) {
    return;
  }
  auto tools = std::make_unique<protocol::Array<protocol::WebMCP::Tool>>();
  tools->push_back(BuildProtocolTool(frame, tool));
  GetFrontend()->toolsRemoved(std::move(tools));
}
}  // namespace blink
