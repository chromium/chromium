// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/inspector/inspector_web_mcp_agent.h"

#include "base/functional/callback_helpers.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/protocol/web_mcp.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_types.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {
InspectorWebMCPAgent::InspectorWebMCPAgent(
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames),
      v8_session_(v8_session),
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
std::unique_ptr<protocol::Value> ParseJSON(const String& value) {
  if (!value) {
    return protocol::DictionaryValue::create();
  }
  std::vector<uint8_t> cbor;

  if (value.Is8Bit()) {
    crdtp::json::ConvertJSONToCBOR(crdtp::span<uint8_t>(value.Span8()), &cbor);
  } else {
    crdtp::json::ConvertJSONToCBOR(crdtp::span<uint16_t>(value.SpanUint16()),
                                   &cbor);
  }
  auto parsed_value = protocol::Value::parseBinary(cbor.data(), cbor.size());
  if (parsed_value) {
    return parsed_value;
  }
  return protocol::StringValue::create(value);
}

std::unique_ptr<protocol::WebMCP::Annotation> BuildAnnotations(
    const mojom::blink::ScriptToolAnnotationsPtr& annotations,
    Element* element) {
  auto builder = protocol::WebMCP::Annotation::create();
  bool has_annotations = false;
  if (annotations) {
    builder.setReadOnly(annotations->read_only);
    builder.setUntrustedContent(annotations->untrusted_content);
    has_annotations = true;
  }
  if (element && element->FastHasAttribute(html_names::kToolautosubmitAttr)) {
    builder.setAutosubmit(true);
    has_annotations = true;
  }
  return has_annotations ? builder.build() : nullptr;
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
    if (auto parsed =
            protocol::DictionaryValue::cast(ParseJSON(input_schema))) {
      tool->setInputSchema(std::move(parsed));
    }
  }

  if (auto annotations = BuildAnnotations(tool_data.ScriptTool().annotations,
                                          tool_data.BackingFormElement())) {
    tool->setAnnotations(std::move(annotations));
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

protocol::Response InspectorWebMCPAgent::disable() {
  enabled_.Set(false);
  instrumenting_agents_->RemoveInspectorWebMCPAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorWebMCPAgent::invokeTool(
    const String& frameId,
    const String& toolName,
    std::unique_ptr<protocol::DictionaryValue> input,
    String* invocationId) {
  LocalFrame* frame = IdentifiersFactory::FrameById(inspected_frames_, frameId);
  if (!frame) {
    return protocol::Response::InvalidParams("No frame for given id found");
  }

  auto* model_context = GetModelContext(frame);
  if (!model_context) {
    return protocol::Response::InvalidParams(
        "No ModelContext for given frame found");
  }

  if (!model_context->GetScriptToolDeclaration(toolName)) {
    return protocol::Response::InvalidParams("Tool not found");
  }

  String input_arguments = "{}";
  if (input) {
    std::vector<uint8_t> cbor;
    input->AppendSerialized(&cbor);
    std::string json;
    crdtp::json::ConvertCBORToJSON(
        crdtp::span<uint8_t>(cbor.data(), cbor.size()), &json);
    input_arguments = String::FromUtf8(json);
  }

  base::UnguessableToken invocation_id = base::UnguessableToken::Create();

  *invocationId = String(invocation_id.ToString());

  frame->GetTaskRunner(TaskType::kInternalInspector)
      ->PostTask(FROM_HERE,
                 blink::BindOnce(base::IgnoreResult(&ModelContext::ExecuteTool),
                                 WrapPersistent(model_context), invocation_id,
                                 toolName, input_arguments,
                                 /*signal=*/nullptr, base::DoNothing()));

  return protocol::Response::Success();
}

protocol::Response InspectorWebMCPAgent::cancelInvocation(
    const String& invocationId) {
  auto invocation_token =
      base::UnguessableToken::DeserializeFromString(invocationId.Ascii());
  if (!invocation_token) {
    return protocol::Response::InvalidParams("Invalid invocation id");
  }

  // Find the model context. Since we don't have the frame ID, we have to
  // iterate over all frames.
  bool cancelled = false;
  for (LocalFrame* frame : *inspected_frames_) {
    if (auto* model_context = GetModelContext(frame)) {
      if (model_context->CancelTool(*invocation_token)) {
        cancelled = true;
        break;
      }
    }
  }

  if (!cancelled) {
    return protocol::Response::InvalidParams(
        "No pending execution for invocation id");
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
  auto tools =
      std::make_unique<protocol::Array<protocol::WebMCP::RemovedTool>>();
  tools->push_back(protocol::WebMCP::RemovedTool::create()
                       .setName(tool.Name())
                       .setFrameId(IdentifiersFactory::FrameId(frame))
                       .build());
  GetFrontend()->toolsRemoved(std::move(tools));
}

void InspectorWebMCPAgent::WebMCPToolExecuted(
    Document* document,
    const String& name,
    const String& input_arguments,
    const base::UnguessableToken& invocation_id) {
  if (LocalFrame* frame = document->GetFrame()) {
    GetFrontend()->toolInvoked(name, IdentifiersFactory::FrameId(frame),
                               String(invocation_id.ToString()),
                               input_arguments);
  }
}

void InspectorWebMCPAgent::WebMCPToolResponded(
    Document* document,
    const String& result,
    const base::UnguessableToken& invocation_id) {
  GetFrontend()->toolResponded(
      String(invocation_id.ToString()),
      protocol::WebMCP::InvocationStatusEnum::Completed, ParseJSON(result));
}

void InspectorWebMCPAgent::WebMCPToolFailed(
    Document* document,
    const ScriptToolError& error,
    const base::UnguessableToken& invocation_id,
    std::optional<std::pair<ScriptValue, ScriptState*>> exception) {
  const char* status = error.code == ScriptToolErrorCode::kToolCancelled
                           ? protocol::WebMCP::InvocationStatusEnum::Canceled
                           : protocol::WebMCP::InvocationStatusEnum::Error;
  std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
      remote_object;
  if (exception && !exception->first.IsEmpty()) {
    ScriptState* script_state = exception->second;
    ScriptState::Scope scope(script_state);
    remote_object = v8_session_->wrapObject(script_state->GetContext(),
                                            exception->first.V8Value(),
                                            v8_inspector::StringView(), false);
  }

  GetFrontend()->toolResponded(String(invocation_id.ToString()), status,
                               nullptr, error.message,
                               std::move(remote_object));
}
}  // namespace blink
