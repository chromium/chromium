// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"

#include "third_party/blink/renderer/core/inspector/protocol/debugger.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace blink {

constexpr char kInstrumentationEventCategoryType[] = "instrumentation:";

constexpr char kWebglErrorNameProperty[] = "webglErrorName";

namespace event_names {

constexpr char kWebglErrorFired[] = "webglErrorFired";
constexpr char kWebglWarningFired[] = "webglWarningFired";
constexpr char kScriptBlockedByCSP[] = "scriptBlockedByCSP";
constexpr char kAudioContextCreated[] = "audioContextCreated";
constexpr char kAudioContextClosed[] = "audioContextClosed";
constexpr char kAudioContextResumed[] = "audioContextResumed";
constexpr char kAudioContextSuspended[] = "audioContextSuspended";
constexpr char kCanvasContextCreated[] = "canvasContextCreated";
constexpr char kScriptFirstStatement[] = "scriptFirstStatement";
constexpr char kSharedStorageWorkletScriptFirstStatement[] =
    "sharedStorageWorkletScriptFirstStatement";

}  // namespace event_names

using Response = protocol::Response;

InspectorEventBreakpointsAgent::InspectorEventBreakpointsAgent(
    v8_inspector::V8InspectorSession* v8_session)
    : v8_session_(v8_session) {}

InspectorEventBreakpointsAgent::~InspectorEventBreakpointsAgent() = default;

void InspectorEventBreakpointsAgent::DidCreateOffscreenCanvasContext() {
  DidCreateCanvasContext();
}

void InspectorEventBreakpointsAgent::DidCreateCanvasContext() {
  if (auto data =
          MaybeBuildBreakpointData(event_names::kCanvasContextCreated)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidFireWebGLError(
    const String& error_name) {
  if (auto data = MaybeBuildBreakpointData(event_names::kWebglErrorFired)) {
    if (!error_name.empty()) {
      data->setString(kWebglErrorNameProperty, error_name);
    }
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidFireWebGLWarning() {
  if (auto data = MaybeBuildBreakpointData(event_names::kWebglWarningFired)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidFireWebGLErrorOrWarning(
    const String& message) {
  if (message.FindIgnoringCase("error") != WTF::kNotFound) {
    DidFireWebGLError(String());
  } else {
    DidFireWebGLWarning();
  }
}

void InspectorEventBreakpointsAgent::ScriptExecutionBlockedByCSP(
    const String& directive_text) {
  if (auto data = MaybeBuildBreakpointData(event_names::kScriptBlockedByCSP)) {
    data->setString("directiveText", directive_text);
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::Will(const probe::ExecuteScript& probe) {
  if (auto data =
          MaybeBuildBreakpointData(event_names::kScriptFirstStatement)) {
    ScheduleAsyncBreakpoint(*data);
    return;
  }

  if (probe.context && probe.context->IsSharedStorageWorkletGlobalScope()) {
    if (auto data = MaybeBuildBreakpointData(
            event_names::kSharedStorageWorkletScriptFirstStatement)) {
      ScheduleAsyncBreakpoint(*data);
    }
  }
}

void InspectorEventBreakpointsAgent::Did(const probe::ExecuteScript& probe) {
  // TODO(caseq): only unschedule if we've previously scheduled?
  UnscheduleAsyncBreakpoint();
}

void InspectorEventBreakpointsAgent::Will(const probe::UserCallback& probe) {
  // Events with targets are handled by DOMDebuggerAgent for now.
  if (probe.event_target) {
    return;
  }
  if (auto data = MaybeBuildBreakpointData(String(probe.name) + ".callback")) {
    ScheduleAsyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::Did(const probe::UserCallback& probe) {
  // TODO(caseq): only unschedule if we've previously scheduled?
  UnscheduleAsyncBreakpoint();
}

void InspectorEventBreakpointsAgent::BreakableLocation(const char* name) {
  if (auto data = MaybeBuildBreakpointData(name)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidCreateAudioContext() {
  if (auto data = MaybeBuildBreakpointData(event_names::kAudioContextCreated)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidCloseAudioContext() {
  if (auto data = MaybeBuildBreakpointData(event_names::kAudioContextClosed)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidResumeAudioContext() {
  if (auto data = MaybeBuildBreakpointData(event_names::kAudioContextResumed)) {
    TriggerSyncBreakpoint(*data);
  }
}

void InspectorEventBreakpointsAgent::DidSuspendAudioContext() {
  if (auto data =
          MaybeBuildBreakpointData(event_names::kAudioContextSuspended)) {
    TriggerSyncBreakpoint(*data);
  }
}

Response InspectorEventBreakpointsAgent::disable() {
  if (IsEnabled()) {
    instrumenting_agents_->RemoveInspectorEventBreakpointsAgent(this);
  }
  event_listener_breakpoints_.Clear();
  agent_state_.ClearAllFields();
  return Response::Success();
}

void InspectorEventBreakpointsAgent::Restore() {
  if (IsEnabled()) {
    instrumenting_agents_->AddInspectorEventBreakpointsAgent(this);
  }
}

Response InspectorEventBreakpointsAgent::setInstrumentationBreakpoint(
    const String& event_name) {
  if (event_name.empty()) {
    return protocol::Response::InvalidParams("Event name is empty");
  }

  if (!IsEnabled()) {
    instrumenting_agents_->AddInspectorEventBreakpointsAgent(this);
  }
  event_listener_breakpoints_.Set(event_name, true);
  return Response::Success();
}

Response InspectorEventBreakpointsAgent::removeInstrumentationBreakpoint(
    const String& event_name) {
  if (event_name.empty()) {
    return protocol::Response::InvalidParams("Event name is empty");
  }
  event_listener_breakpoints_.Clear(event_name);
  if (!IsEnabled()) {
    instrumenting_agents_->RemoveInspectorEventBreakpointsAgent(this);
  }
  return Response::Success();
}

bool InspectorEventBreakpointsAgent::IsEnabled() const {
  return !event_listener_breakpoints_.IsEmpty();
}

std::unique_ptr<protocol::DictionaryValue>
InspectorEventBreakpointsAgent::MaybeBuildBreakpointData(
    const String& event_name) {
  if (!event_listener_breakpoints_.Get(event_name)) {
    return nullptr;
  }

  auto event_data = protocol::DictionaryValue::create();
  const String full_event_name =
      String(kInstrumentationEventCategoryType) + event_name;
  event_data->setString("eventName", full_event_name);

  return event_data;
}

namespace {

std::vector<uint8_t> JsonFromDictionary(const protocol::DictionaryValue& dict) {
  std::vector<uint8_t> json;
  crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(dict.Serialize()), &json);

  return json;
}

}  // namespace

void InspectorEventBreakpointsAgent::TriggerSyncBreakpoint(
    const protocol::DictionaryValue& breakpoint_data) {
  std::vector<uint8_t> json = JsonFromDictionary(breakpoint_data);
  v8_session_->breakProgram(
      ToV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                  ReasonEnum::EventListener),
      v8_inspector::StringView(json.data(), json.size()));
}

void InspectorEventBreakpointsAgent::ScheduleAsyncBreakpoint(
    const protocol::DictionaryValue& breakpoint_data) {
  std::vector<uint8_t> json = JsonFromDictionary(breakpoint_data);
  v8_session_->schedulePauseOnNextStatement(
      ToV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                  ReasonEnum::EventListener),
      v8_inspector::StringView(json.data(), json.size()));
}

void InspectorEventBreakpointsAgent::UnscheduleAsyncBreakpoint() {
  v8_session_->cancelPauseOnNextStatement();
}

}  // namespace blink
