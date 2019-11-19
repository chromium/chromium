/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

enum DOMBreakpointType {
  SubtreeModified = 0,
  AttributeModified,
  NodeRemoved,
  DOMBreakpointTypesCount
};

static const char listenerEventCategoryType[] = "listener:";
static const char instrumentationEventCategoryType[] = "instrumentation:";

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

static const char kWebglErrorFiredEventName[] = "webglErrorFired";
static const char kWebglWarningFiredEventName[] = "webglWarningFired";
static const char kWebglErrorNameProperty[] = "webglErrorName";
static const char kScriptBlockedByCSPEventName[] = "scriptBlockedByCSP";
static const char kCanvasContextCreatedEventName[] = "canvasContextCreated";
static const char kAudioContextCreatedEventName[] = "audioContextCreated";
static const char kAudioContextClosedEventName[] = "audioContextClosed";
static const char kAudioContextResumedEventName[] = "audioContextResumed";
static const char kAudioContextSuspendedEventName[] = "audioContextSuspended";
}  // namespace

namespace blink {
using protocol::Maybe;
using protocol::Response;
namespace {
// Returns the key that we use to identify the brekpoint in
// event_listener_breakpoints_. |target_name| may be "", in which case
// we'll match any target.
WTF::String EventListenerBreakpointKey(const WTF::String& event_name,
                                       const WTF::String& target_name) {
  if (target_name.IsEmpty() || target_name == "*")
    return event_name + "$$" + "*";
  return event_name + "$$" + target_name.LowerASCII();
}
}  // namespace

// static
void InspectorDOMDebuggerAgent::CollectEventListeners(
    v8::Isolate* isolate,
    EventTarget* target,
    v8::Local<v8::Value> target_wrapper,
    Node* target_node,
    bool report_for_all_contexts,
    V8EventListenerInfoList* event_information) {
  if (!target->GetExecutionContext())
    return;

  ExecutionContext* execution_context = target->GetExecutionContext();

  // Nodes and their Listeners for the concerned event types (order is top to
  // bottom).
  Vector<AtomicString> event_types = target->EventTypes();
  for (wtf_size_t j = 0; j < event_types.size(); ++j) {
    AtomicString& type = event_types[j];
    EventListenerVector* listeners = target->GetEventListeners(type);
    if (!listeners)
      continue;
    for (wtf_size_t k = 0; k < listeners->size(); ++k) {
      EventListener* event_listener = listeners->at(k).Callback();
      JSBasedEventListener* v8_event_listener =
          DynamicTo<JSBasedEventListener>(event_listener);
      if (!v8_event_listener)
        continue;
      v8::Local<v8::Context> context = ToV8Context(
          execution_context, v8_event_listener->GetWorldForInspector());
      // Optionally hide listeners from other contexts.
      if (!report_for_all_contexts && context != isolate->GetCurrentContext())
        continue;
      v8::Local<v8::Value> handler =
          v8_event_listener->GetListenerObject(*target);
      if (handler.IsEmpty() || !handler->IsObject())
        continue;
      v8::Local<v8::Value> effective_function =
          v8_event_listener->GetEffectiveFunction(*target);
      if (!effective_function->IsFunction())
        continue;
      DOMNodeId backend_node_id = 0;
      if (target_node) {
        backend_node_id = DOMNodeIds::IdForNode(target_node);
        target_wrapper = NodeV8Value(
            report_for_all_contexts ? context : isolate->GetCurrentContext(),
            target_node);
      }
      event_information->push_back(V8EventListenerInfo(
          type, listeners->at(k).Capture(), listeners->at(k).Passive(),
          listeners->at(k).Once(), handler.As<v8::Object>(),
          effective_function.As<v8::Function>(), backend_node_id));
    }
  }
}

// static
void InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    V8EventListenerInfoList* event_information) {
  InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
      isolate, value, 1, false, event_information);
}

static bool FilterNodesWithListeners(Node* node) {
  Vector<AtomicString> event_types = node->EventTypes();
  for (wtf_size_t j = 0; j < event_types.size(); ++j) {
    EventListenerVector* listeners = node->GetEventListeners(event_types[j]);
    if (listeners && listeners->size())
      return true;
  }
  return false;
}

// static
void InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    int depth,
    bool pierce,
    V8EventListenerInfoList* event_information) {
  // Special-case nodes, respect depth and pierce parameters in case of nodes.
  Node* node = V8Node::ToImplWithTypeCheck(isolate, value);
  if (node) {
    if (depth < 0)
      depth = INT_MAX;
    HeapVector<Member<Node>> nodes;
    InspectorDOMAgent::CollectNodes(
        node, depth, pierce, WTF::BindRepeating(&FilterNodesWithListeners),
        &nodes);
    for (Node* n : nodes) {
      // We are only interested in listeners from the current context.
      CollectEventListeners(isolate, n, v8::Local<v8::Value>(), n, pierce,
                            event_information);
    }
    return;
  }

  EventTarget* target = V8EventTarget::ToImplWithTypeCheck(isolate, value);
  // We need to handle LocalDOMWindow specially, because LocalDOMWindow wrapper
  // exists on prototype chain.
  if (!target)
    target = ToDOMWindow(isolate, value);
  if (target) {
    CollectEventListeners(isolate, target, value, nullptr, false,
                          event_information);
  }
}

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(
    v8::Isolate* isolate,
    InspectorDOMAgent* dom_agent,
    v8_inspector::V8InspectorSession* v8_session)
    : isolate_(isolate),
      dom_agent_(dom_agent),
      v8_session_(v8_session),
      enabled_(&agent_state_, /*default_value=*/false),
      pause_on_all_xhrs_(&agent_state_, /*default_value=*/false),
      xhr_breakpoints_(&agent_state_, /*default_value=*/false),
      event_listener_breakpoints_(&agent_state_, /*default_value*/ false) {}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent() = default;

void InspectorDOMDebuggerAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_agent_);
  visitor->Trace(dom_breakpoints_);
  InspectorBaseAgent::Trace(visitor);
}

Response InspectorDOMDebuggerAgent::disable() {
  SetEnabled(false);
  dom_breakpoints_.clear();
  agent_state_.ClearAllFields();
  return Response::OK();
}

void InspectorDOMDebuggerAgent::Restore() {
  if (enabled_.Get())
    instrumenting_agents_->AddInspectorDOMDebuggerAgent(this);
}

Response InspectorDOMDebuggerAgent::setEventListenerBreakpoint(
    const String& event_name,
    Maybe<String> target_name) {
  return SetBreakpoint(String(listenerEventCategoryType) + event_name,
                       target_name.fromMaybe(String()));
}

Response InspectorDOMDebuggerAgent::setInstrumentationBreakpoint(
    const String& event_name) {
  return SetBreakpoint(String(instrumentationEventCategoryType) + event_name,
                       String());
}

Response InspectorDOMDebuggerAgent::SetBreakpoint(const String& event_name,
                                                  const String& target_name) {
  if (event_name.IsEmpty())
    return Response::Error("Event name is empty");
  event_listener_breakpoints_.Set(
      EventListenerBreakpointKey(event_name, target_name), true);
  DidAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeEventListenerBreakpoint(
    const String& event_name,
    Maybe<String> target_name) {
  return RemoveBreakpoint(String(listenerEventCategoryType) + event_name,
                          target_name.fromMaybe(String()));
}

Response InspectorDOMDebuggerAgent::removeInstrumentationBreakpoint(
    const String& event_name) {
  return RemoveBreakpoint(String(instrumentationEventCategoryType) + event_name,
                          String());
}

Response InspectorDOMDebuggerAgent::RemoveBreakpoint(
    const String& event_name,
    const String& target_name) {
  if (event_name.IsEmpty())
    return Response::Error("Event name is empty");
  event_listener_breakpoints_.Clear(
      EventListenerBreakpointKey(event_name, target_name));
  DidRemoveBreakpoint();
  return Response::OK();
}

void InspectorDOMDebuggerAgent::DidInvalidateStyleAttr(Node* node) {
  if (HasBreakpoint(node, AttributeModified))
    BreakProgramOnDOMEvent(node, AttributeModified, false);
}

void InspectorDOMDebuggerAgent::DidInsertDOMNode(Node* node) {
  if (dom_breakpoints_.size()) {
    uint32_t mask =
        dom_breakpoints_.at(InspectorDOMAgent::InnerParentNode(node));
    uint32_t inheritable_types_mask =
        (mask | (mask >> domBreakpointDerivedTypeShift)) &
        inheritableDOMBreakpointTypesMask;
    if (inheritable_types_mask)
      UpdateSubtreeBreakpoints(node, inheritable_types_mask, true);
  }
}

void InspectorDOMDebuggerAgent::DidRemoveDOMNode(Node* node) {
  if (dom_breakpoints_.size()) {
    // Remove subtree breakpoints.
    dom_breakpoints_.erase(node);
    HeapVector<Member<Node>> stack(1, InspectorDOMAgent::InnerFirstChild(node));
    do {
      Node* node = stack.back();
      stack.pop_back();
      if (!node)
        continue;
      dom_breakpoints_.erase(node);
      stack.push_back(InspectorDOMAgent::InnerFirstChild(node));
      stack.push_back(InspectorDOMAgent::InnerNextSibling(node));
    } while (!stack.IsEmpty());
  }
}

static Response DomTypeForName(const String& type_string, int& type) {
  if (type_string == "subtree-modified") {
    type = SubtreeModified;
    return Response::OK();
  }
  if (type_string == "attribute-modified") {
    type = AttributeModified;
    return Response::OK();
  }
  if (type_string == "node-removed") {
    type = NodeRemoved;
    return Response::OK();
  }
  return Response::Error(String("Unknown DOM breakpoint type: " + type_string));
}

static String DomTypeName(int type) {
  switch (type) {
    case SubtreeModified:
      return "subtree-modified";
    case AttributeModified:
      return "attribute-modified";
    case NodeRemoved:
      return "node-removed";
    default:
      break;
  }
  return "";
}

Response InspectorDOMDebuggerAgent::setDOMBreakpoint(
    int node_id,
    const String& type_string) {
  Node* node = nullptr;
  Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  int type = -1;
  response = DomTypeForName(type_string, type);
  if (!response.isSuccess())
    return response;

  uint32_t root_bit = 1 << type;
  dom_breakpoints_.Set(node, dom_breakpoints_.at(node) | root_bit);
  if (root_bit & inheritableDOMBreakpointTypesMask) {
    for (Node* child = InspectorDOMAgent::InnerFirstChild(node); child;
         child = InspectorDOMAgent::InnerNextSibling(child))
      UpdateSubtreeBreakpoints(child, root_bit, true);
  }
  DidAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeDOMBreakpoint(
    int node_id,
    const String& type_string) {
  Node* node = nullptr;
  Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  int type = -1;
  response = DomTypeForName(type_string, type);
  if (!response.isSuccess())
    return response;

  uint32_t root_bit = 1 << type;
  uint32_t mask = dom_breakpoints_.at(node) & ~root_bit;
  if (mask)
    dom_breakpoints_.Set(node, mask);
  else
    dom_breakpoints_.erase(node);

  if ((root_bit & inheritableDOMBreakpointTypesMask) &&
      !(mask & (root_bit << domBreakpointDerivedTypeShift))) {
    for (Node* child = InspectorDOMAgent::InnerFirstChild(node); child;
         child = InspectorDOMAgent::InnerNextSibling(child))
      UpdateSubtreeBreakpoints(child, root_bit, false);
  }
  DidRemoveBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::getEventListeners(
    const String& object_id,
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::Array<protocol::DOMDebugger::EventListener>>*
        listeners_array) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> object;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  std::unique_ptr<v8_inspector::StringBuffer> object_group;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &object, &context, &object_group)) {
    return Response::Error(ToCoreString(std::move(error)));
  }
  v8::Context::Scope scope(context);
  V8EventListenerInfoList event_information;
  InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
      context->GetIsolate(), object, depth.fromMaybe(1),
      pierce.fromMaybe(false), &event_information);
  *listeners_array = BuildObjectsForEventListeners(event_information, context,
                                                   object_group->string());
  return Response::OK();
}

std::unique_ptr<protocol::Array<protocol::DOMDebugger::EventListener>>
InspectorDOMDebuggerAgent::BuildObjectsForEventListeners(
    const V8EventListenerInfoList& event_information,
    v8::Local<v8::Context> context,
    const v8_inspector::StringView& object_group_id) {
  auto listeners_array =
      std::make_unique<protocol::Array<protocol::DOMDebugger::EventListener>>();
  // Make sure listeners with |use_capture| true come first because they have
  // precedence.
  for (const auto& info : event_information) {
    if (!info.use_capture)
      continue;
    std::unique_ptr<protocol::DOMDebugger::EventListener> listener_object =
        BuildObjectForEventListener(context, info, object_group_id);
    if (listener_object)
      listeners_array->emplace_back(std::move(listener_object));
  }
  for (const auto& info : event_information) {
    if (info.use_capture)
      continue;
    std::unique_ptr<protocol::DOMDebugger::EventListener> listener_object =
        BuildObjectForEventListener(context, info, object_group_id);
    if (listener_object)
      listeners_array->emplace_back(std::move(listener_object));
  }
  return listeners_array;
}

std::unique_ptr<protocol::DOMDebugger::EventListener>
InspectorDOMDebuggerAgent::BuildObjectForEventListener(
    v8::Local<v8::Context> context,
    const V8EventListenerInfo& info,
    const v8_inspector::StringView& object_group_id) {
  if (info.handler.IsEmpty())
    return nullptr;

  v8::Local<v8::Function> function = info.effective_function;
  std::unique_ptr<protocol::DOMDebugger::EventListener> value =
      protocol::DOMDebugger::EventListener::create()
          .setType(info.event_type)
          .setUseCapture(info.use_capture)
          .setPassive(info.passive)
          .setOnce(info.once)
          .setScriptId(String::Number(function->ScriptId()))
          .setLineNumber(function->GetScriptLineNumber())
          .setColumnNumber(function->GetScriptColumnNumber())
          .build();
  if (object_group_id.length()) {
    value->setHandler(v8_session_->wrapObject(
        context, function, object_group_id, false /* generatePreview */));
    value->setOriginalHandler(v8_session_->wrapObject(
        context, info.handler, object_group_id, false /* generatePreview */));
    if (info.backend_node_id)
      value->setBackendNodeId(static_cast<int>(info.backend_node_id));
  }
  return value;
}

void InspectorDOMDebuggerAgent::AllowNativeBreakpoint(
    const String& breakpoint_name,
    const String* target_name,
    bool sync) {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(breakpoint_name, target_name), sync);
}

void InspectorDOMDebuggerAgent::WillInsertDOMNode(Node* parent) {
  if (HasBreakpoint(parent, SubtreeModified))
    BreakProgramOnDOMEvent(parent, SubtreeModified, true);
}

void InspectorDOMDebuggerAgent::WillRemoveDOMNode(Node* node) {
  Node* parent_node = InspectorDOMAgent::InnerParentNode(node);
  if (HasBreakpoint(node, NodeRemoved))
    BreakProgramOnDOMEvent(node, NodeRemoved, false);
  else if (parent_node && HasBreakpoint(parent_node, SubtreeModified))
    BreakProgramOnDOMEvent(node, SubtreeModified, false);
  DidRemoveDOMNode(node);
}

void InspectorDOMDebuggerAgent::WillModifyDOMAttr(Element* element,
                                                  const AtomicString&,
                                                  const AtomicString&) {
  if (HasBreakpoint(element, AttributeModified))
    BreakProgramOnDOMEvent(element, AttributeModified, false);
}

void InspectorDOMDebuggerAgent::BreakProgramOnDOMEvent(Node* target,
                                                       int breakpoint_type,
                                                       bool insertion) {
  DCHECK(HasBreakpoint(target, breakpoint_type));
  std::unique_ptr<protocol::DictionaryValue> description =
      protocol::DictionaryValue::create();

  Node* breakpoint_owner = target;
  if ((1 << breakpoint_type) & inheritableDOMBreakpointTypesMask) {
    // For inheritable breakpoint types, target node isn't always the same as
    // the node that owns a breakpoint.  Target node may be unknown to frontend,
    // so we need to push it first.
    description->setInteger("targetNodeId",
                            dom_agent_->PushNodePathToFrontend(target));

    // Find breakpoint owner node.
    if (!insertion)
      breakpoint_owner = InspectorDOMAgent::InnerParentNode(target);
    DCHECK(breakpoint_owner);
    while (!(dom_breakpoints_.at(breakpoint_owner) & (1 << breakpoint_type))) {
      Node* parent_node = InspectorDOMAgent::InnerParentNode(breakpoint_owner);
      if (!parent_node)
        break;
      breakpoint_owner = parent_node;
    }

    if (breakpoint_type == SubtreeModified)
      description->setBoolean("insertion", insertion);
  }

  int breakpoint_owner_node_id = dom_agent_->BoundNodeId(breakpoint_owner);
  DCHECK(breakpoint_owner_node_id);
  description->setInteger("nodeId", breakpoint_owner_node_id);
  description->setString("type", DomTypeName(breakpoint_type));
  String json = description->toJSONString();
  v8_session_->breakProgram(
      ToV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::DOM),
      ToV8InspectorStringView(json));
}

bool InspectorDOMDebuggerAgent::HasBreakpoint(Node* node, int type) {
  if (!dom_agent_->Enabled())
    return false;
  uint32_t root_bit = 1 << type;
  uint32_t derived_bit = root_bit << domBreakpointDerivedTypeShift;
  return dom_breakpoints_.at(node) & (root_bit | derived_bit);
}

void InspectorDOMDebuggerAgent::UpdateSubtreeBreakpoints(Node* node,
                                                         uint32_t root_mask,
                                                         bool set) {
  uint32_t old_mask = dom_breakpoints_.at(node);
  uint32_t derived_mask = root_mask << domBreakpointDerivedTypeShift;
  uint32_t new_mask = set ? old_mask | derived_mask : old_mask & ~derived_mask;
  if (new_mask)
    dom_breakpoints_.Set(node, new_mask);
  else
    dom_breakpoints_.erase(node);

  uint32_t new_root_mask = root_mask & ~new_mask;
  if (!new_root_mask)
    return;

  for (Node* child = InspectorDOMAgent::InnerFirstChild(node); child;
       child = InspectorDOMAgent::InnerNextSibling(child))
    UpdateSubtreeBreakpoints(child, new_root_mask, set);
}

void InspectorDOMDebuggerAgent::PauseOnNativeEventIfNeeded(
    std::unique_ptr<protocol::DictionaryValue> event_data,
    bool synchronous) {
  if (!event_data)
    return;
  String json = event_data->toJSONString();
  if (synchronous)
    v8_session_->breakProgram(
        ToV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                    ReasonEnum::EventListener),
        ToV8InspectorStringView(json));
  else
    v8_session_->schedulePauseOnNextStatement(
        ToV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                    ReasonEnum::EventListener),
        ToV8InspectorStringView(json));
}

std::unique_ptr<protocol::DictionaryValue>
InspectorDOMDebuggerAgent::PreparePauseOnNativeEventData(
    const String& event_name,
    const String* target_name) {
  String full_event_name = (target_name ? listenerEventCategoryType
                                        : instrumentationEventCategoryType) +
                           event_name;

  bool match = event_listener_breakpoints_.Get(
      EventListenerBreakpointKey(full_event_name, "*"));
  if (!match && target_name) {
    match = event_listener_breakpoints_.Get(
        EventListenerBreakpointKey(full_event_name, *target_name));
  }
  if (!match)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> event_data =
      protocol::DictionaryValue::create();
  event_data->setString("eventName", full_event_name);
  if (target_name)
    event_data->setString("targetName", *target_name);
  return event_data;
}

void InspectorDOMDebuggerAgent::DidFireWebGLError(const String& error_name) {
  std::unique_ptr<protocol::DictionaryValue> event_data =
      PreparePauseOnNativeEventData(kWebglErrorFiredEventName, nullptr);
  if (!event_data)
    return;
  if (!error_name.IsEmpty())
    event_data->setString(kWebglErrorNameProperty, error_name);
  PauseOnNativeEventIfNeeded(std::move(event_data), true);
}

void InspectorDOMDebuggerAgent::DidFireWebGLWarning() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kWebglWarningFiredEventName, nullptr),
      true);
}

void InspectorDOMDebuggerAgent::DidFireWebGLErrorOrWarning(
    const String& message) {
  if (message.FindIgnoringCase("error") != WTF::kNotFound)
    DidFireWebGLError(String());
  else
    DidFireWebGLWarning();
}

void InspectorDOMDebuggerAgent::CancelNativeBreakpoint() {
  v8_session_->cancelPauseOnNextStatement();
}

void InspectorDOMDebuggerAgent::ScriptExecutionBlockedByCSP(
    const String& directive_text) {
  std::unique_ptr<protocol::DictionaryValue> event_data =
      PreparePauseOnNativeEventData(kScriptBlockedByCSPEventName, nullptr);
  if (!event_data)
    return;
  event_data->setString("directiveText", directive_text);
  PauseOnNativeEventIfNeeded(std::move(event_data), true);
}

void InspectorDOMDebuggerAgent::Will(const probe::ExecuteScript& probe) {
  AllowNativeBreakpoint("scriptFirstStatement", nullptr, false);
}

void InspectorDOMDebuggerAgent::Did(const probe::ExecuteScript& probe) {
  CancelNativeBreakpoint();
}

void InspectorDOMDebuggerAgent::Will(const probe::UserCallback& probe) {
  String name = probe.name ? String(probe.name) : probe.atomic_name;
  if (probe.event_target) {
    Node* node = probe.event_target->ToNode();
    String target_name =
        node ? node->nodeName() : probe.event_target->InterfaceName();
    AllowNativeBreakpoint(name, &target_name, false);
    return;
  }
  AllowNativeBreakpoint(name + ".callback", nullptr, false);
}

void InspectorDOMDebuggerAgent::Did(const probe::UserCallback& probe) {
  CancelNativeBreakpoint();
}

void InspectorDOMDebuggerAgent::BreakableLocation(const char* name) {
  AllowNativeBreakpoint(name, nullptr, true);
}

Response InspectorDOMDebuggerAgent::setXHRBreakpoint(const String& url) {
  if (url.IsEmpty())
    pause_on_all_xhrs_.Set(true);
  else
    xhr_breakpoints_.Set(url, true);
  DidAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeXHRBreakpoint(const String& url) {
  if (url.IsEmpty())
    pause_on_all_xhrs_.Set(false);
  else
    xhr_breakpoints_.Clear(url);
  DidRemoveBreakpoint();
  return Response::OK();
}

// Returns the breakpoint url if a match is found, or WTF::String().
String InspectorDOMDebuggerAgent::MatchXHRBreakpoints(const String& url) const {
  if (pause_on_all_xhrs_.Get())
    return "";
  for (const WTF::String& breakpoint : xhr_breakpoints_.Keys()) {
    if (url.Contains(breakpoint))
      return breakpoint;
  }
  return WTF::String();
}

void InspectorDOMDebuggerAgent::WillSendXMLHttpOrFetchNetworkRequest(
    const String& url) {
  String breakpoint_url = MatchXHRBreakpoints(url);
  if (breakpoint_url.IsNull())
    return;

  std::unique_ptr<protocol::DictionaryValue> event_data =
      protocol::DictionaryValue::create();
  event_data->setString("breakpointURL", breakpoint_url);
  event_data->setString("url", url);
  String json = event_data->toJSONString();
  v8_session_->breakProgram(
      ToV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::XHR),
      ToV8InspectorStringView(json));
}

void InspectorDOMDebuggerAgent::DidCreateCanvasContext() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kCanvasContextCreatedEventName, nullptr),
      true);
}

void InspectorDOMDebuggerAgent::DidAddBreakpoint() {
  if (enabled_.Get())
    return;
  SetEnabled(true);
}

void InspectorDOMDebuggerAgent::DidRemoveBreakpoint() {
  if (!dom_breakpoints_.IsEmpty())
    return;
  if (!event_listener_breakpoints_.IsEmpty())
    return;
  if (!xhr_breakpoints_.IsEmpty())
    return;
  if (pause_on_all_xhrs_.Get())
    return;
  SetEnabled(false);
}

void InspectorDOMDebuggerAgent::SetEnabled(bool enabled) {
  enabled_.Set(enabled);
  if (enabled)
    instrumenting_agents_->AddInspectorDOMDebuggerAgent(this);
  else
    instrumenting_agents_->RemoveInspectorDOMDebuggerAgent(this);
}

void InspectorDOMDebuggerAgent::DidCommitLoadForLocalFrame(LocalFrame*) {
  dom_breakpoints_.clear();
}

void InspectorDOMDebuggerAgent::DidCreateAudioContext() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kAudioContextCreatedEventName, nullptr),
      true);
}

void InspectorDOMDebuggerAgent::DidCloseAudioContext() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kAudioContextClosedEventName, nullptr),
      true);
}

void InspectorDOMDebuggerAgent::DidResumeAudioContext() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kAudioContextResumedEventName, nullptr),
      true);
}

void InspectorDOMDebuggerAgent::DidSuspendAudioContext() {
  PauseOnNativeEventIfNeeded(
      PreparePauseOnNativeEventData(kAudioContextSuspendedEventName, nullptr),
      true);
}

}  // namespace blink
