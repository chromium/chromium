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
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy_violation_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/inspector_protocol/crdtp/json.h"
using crdtp::SpanFrom;
using crdtp::json::ConvertCBORToJSON;

namespace {

enum DOMBreakpointType {
  SubtreeModified = 0,
  AttributeModified,
  NodeRemoved,
  DOMBreakpointTypesCount
};

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

const char kListenerEventCategoryType[] = "listener:";

}  // namespace

namespace blink {
using protocol::Maybe;
namespace {
// Returns the key that we use to identify the brekpoint in
// event_listener_breakpoints_. |target_name| may be "", in which case
// we'll match any target.
WTF::String EventListenerBreakpointKey(const WTF::String& event_name,
                                       const WTF::String& target_name) {
  if (target_name.empty() || target_name == "*")
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
  for (AtomicString& type : event_types) {
    // We need to clone the EventListenerVector because `GetEffectiveFunction`
    // can execute script which may invalidate the iterator.
    EventListenerVector listeners;
    if (auto* registered_listeners = target->GetEventListeners(type)) {
      listeners = *registered_listeners;
    } else {
      continue;
    }
    for (auto& registered_event_listener : listeners) {
      if (registered_event_listener->Removed()) {
        continue;
      }
      EventListener* event_listener = registered_event_listener->Callback();
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
        backend_node_id = target_node->GetDomNodeId();
        target_wrapper = NodeV8Value(
            report_for_all_contexts ? context : isolate->GetCurrentContext(),
            target_node);
      }
      event_information->push_back(V8EventListenerInfo(
          type, registered_event_listener->Capture(),
          registered_event_listener->Passive(),
          registered_event_listener->Once(), handler.As<v8::Object>(),
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
      isolate, value, 1, false, InspectorDOMAgent::IncludeWhitespaceEnum::NONE,
      event_information);
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
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace,
    V8EventListenerInfoList* event_information) {
  // Special-case nodes, respect depth and pierce parameters in case of nodes.
  Node* node = V8Node::ToWrappable(isolate, value);
  if (node) {
    if (depth < 0)
      depth = INT_MAX;
    HeapVector<Member<Node>> nodes;
    InspectorDOMAgent::CollectNodes(
        node, depth, pierce, include_whitespace,
        WTF::BindRepeating(&FilterNodesWithListeners), &nodes);
    for (Node* n : nodes) {
      // We are only interested in listeners from the current context.
      CollectEventListeners(isolate, n, v8::Local<v8::Value>(), n, pierce,
                            event_information);
    }
    return;
  }

  if (EventTarget* target = V8EventTarget::ToWrappable(isolate, value)) {
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
      event_listener_breakpoints_(&agent_state_, /*default_value*/ false),
      csp_violation_breakpoints_(&agent_state_, /*default_value*/ false) {
  DCHECK(dom_agent);
}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent() = default;

void InspectorDOMDebuggerAgent::Trace(Visitor* visitor) const {
  visitor->Trace(dom_agent_);
  visitor->Trace(dom_breakpoints_);
  InspectorBaseAgent::Trace(visitor);
}

protocol::Response InspectorDOMDebuggerAgent::disable() {
  SetEnabled(false);
  dom_breakpoints_.clear();
  agent_state_.ClearAllFields();
  return protocol::Response::Success();
}

void InspectorDOMDebuggerAgent::Restore() {
  if (enabled_.Get())
    instrumenting_agents_->AddInspectorDOMDebuggerAgent(this);
}

protocol::Response InspectorDOMDebuggerAgent::setEventListenerBreakpoint(
    const String& event_name,
    Maybe<String> target_name) {
  return SetBreakpoint(event_name, target_name.value_or(String()));
}

protocol::Response InspectorDOMDebuggerAgent::SetBreakpoint(
    const String& event_name,
    const String& target_name) {
  if (event_name.empty())
    return protocol::Response::ServerError("Event name is empty");
  event_listener_breakpoints_.Set(
      EventListenerBreakpointKey(event_name, target_name), true);
  DidAddBreakpoint();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMDebuggerAgent::removeEventListenerBreakpoint(
    const String& event_name,
    Maybe<String> target_name) {
  return RemoveBreakpoint(event_name, target_name.value_or(String()));
}

protocol::Response InspectorDOMDebuggerAgent::RemoveBreakpoint(
    const String& event_name,
    const String& target_name) {
  if (event_name.empty())
    return protocol::Response::ServerError("Event name is empty");
  event_listener_breakpoints_.Clear(
      EventListenerBreakpointKey(event_name, target_name));
  DidRemoveBreakpoint();
  return protocol::Response::Success();
}

void InspectorDOMDebuggerAgent::DidInvalidateStyleAttr(Node* node) {
  if (HasBreakpoint(node, AttributeModified))
    BreakProgramOnDOMEvent(node, AttributeModified, false);
}

void InspectorDOMDebuggerAgent::DidInsertDOMNode(Node* node) {
  if (dom_breakpoints_.size()) {
    uint32_t mask =
        FindBreakpointMask(InspectorDOMAgent::InnerParentNode(node));
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
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
        dom_agent_->IncludeWhitespace();
    HeapVector<Member<Node>> stack(
        1, InspectorDOMAgent::InnerFirstChild(node, include_whitespace));
    do {
      Node* child_node = stack.back();
      stack.pop_back();
      if (!child_node)
        continue;
      dom_breakpoints_.erase(child_node);
      stack.push_back(
          InspectorDOMAgent::InnerFirstChild(child_node, include_whitespace));
      stack.push_back(
          InspectorDOMAgent::InnerNextSibling(child_node, include_whitespace));
    } while (!stack.empty());
  }
}

static protocol::Response DomTypeForName(const String& type_string, int& type) {
  if (type_string == "subtree-modified") {
    type = SubtreeModified;
    return protocol::Response::Success();
  }
  if (type_string == "attribute-modified") {
    type = AttributeModified;
    return protocol::Response::Success();
  }
  if (type_string == "node-removed") {
    type = NodeRemoved;
    return protocol::Response::Success();
  }
  return protocol::Response::ServerError(
      String("Unknown DOM breakpoint type: " + type_string).Utf8());
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
  return WTF::g_empty_string;
}

bool IsValidViolationType(const String& violationString) {
  if (violationString ==
      protocol::DOMDebugger::CSPViolationTypeEnum::TrustedtypeSinkViolation) {
    return true;
  }
  if (violationString ==
      protocol::DOMDebugger::CSPViolationTypeEnum::TrustedtypePolicyViolation) {
    return true;
  }
  return false;
}

protocol::Response InspectorDOMDebuggerAgent::setBreakOnCSPViolation(
    std::unique_ptr<protocol::Array<String>> violationTypes) {
  csp_violation_breakpoints_.Clear();
  if (violationTypes->empty()) {
    DidRemoveBreakpoint();
    return protocol::Response::Success();
  }
  for (const auto& violationString : *violationTypes) {
    if (IsValidViolationType(violationString)) {
      csp_violation_breakpoints_.Set(violationString, true);
    } else {
      csp_violation_breakpoints_.Clear();
      DidRemoveBreakpoint();
      return protocol::Response::InvalidParams("Invalid violation type");
    }
  }
  DidAddBreakpoint();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMDebuggerAgent::setDOMBreakpoint(
    int node_id,
    const String& type_string) {
  Node* node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  int type = -1;
  response = DomTypeForName(type_string, type);
  if (!response.IsSuccess())
    return response;

  uint32_t root_bit = 1 << type;
  dom_breakpoints_.Set(node, FindBreakpointMask(node) | root_bit);
  if (root_bit & inheritableDOMBreakpointTypesMask) {
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
        dom_agent_->IncludeWhitespace();
    for (Node* child =
             InspectorDOMAgent::InnerFirstChild(node, include_whitespace);
         child;
         child = InspectorDOMAgent::InnerNextSibling(child, include_whitespace))
      UpdateSubtreeBreakpoints(child, root_bit, true);
  }
  DidAddBreakpoint();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMDebuggerAgent::removeDOMBreakpoint(
    int node_id,
    const String& type_string) {
  Node* node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  int type = -1;
  response = DomTypeForName(type_string, type);
  if (!response.IsSuccess())
    return response;

  uint32_t root_bit = 1 << type;
  uint32_t mask = FindBreakpointMask(node) & ~root_bit;
  if (mask)
    dom_breakpoints_.Set(node, mask);
  else
    dom_breakpoints_.erase(node);

  if ((root_bit & inheritableDOMBreakpointTypesMask) &&
      !(mask & (root_bit << domBreakpointDerivedTypeShift))) {
    InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
        dom_agent_->IncludeWhitespace();
    for (Node* child =
             InspectorDOMAgent::InnerFirstChild(node, include_whitespace);
         child;
         child = InspectorDOMAgent::InnerNextSibling(child, include_whitespace))
      UpdateSubtreeBreakpoints(child, root_bit, false);
  }
  DidRemoveBreakpoint();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMDebuggerAgent::getEventListeners(
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
    return protocol::Response::ServerError(
        ToCoreString(std::move(error)).Utf8());
  }
  v8::Context::Scope scope(context);
  V8EventListenerInfoList event_information;
  InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
      context->GetIsolate(), object, depth.value_or(1), pierce.value_or(false),
      dom_agent_->IncludeWhitespace(), &event_information);
  *listeners_array = BuildObjectsForEventListeners(event_information, context,
                                                   object_group->string());
  return protocol::Response::Success();
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
  }
  if (info.backend_node_id)
    value->setBackendNodeId(static_cast<int>(info.backend_node_id));
  return value;
}

void InspectorDOMDebuggerAgent::WillInsertDOMNode(Node* parent) {
  if (HasBreakpoint(parent, SubtreeModified))
    BreakProgramOnDOMEvent(parent, SubtreeModified, true);
}

void InspectorDOMDebuggerAgent::CharacterDataModified(CharacterData* node) {
  if (HasBreakpoint(node, SubtreeModified))
    BreakProgramOnDOMEvent(node, SubtreeModified, false);
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
    while (!(FindBreakpointMask(breakpoint_owner) & (1 << breakpoint_type))) {
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
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(description->Serialize()), &json);
  v8_session_->breakProgram(
      ToV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::DOM),
      v8_inspector::StringView(json.data(), json.size()));
}

bool InspectorDOMDebuggerAgent::HasBreakpoint(Node* node, int type) const {
  if (!dom_agent_->Enabled())
    return false;
  uint32_t root_bit = 1 << type;
  uint32_t derived_bit = root_bit << domBreakpointDerivedTypeShift;
  return FindBreakpointMask(node) & (root_bit | derived_bit);
}

uint32_t InspectorDOMDebuggerAgent::FindBreakpointMask(Node* node) const {
  auto it = dom_breakpoints_.find(node);
  return it != dom_breakpoints_.end() ? it->value : 0;
}

void InspectorDOMDebuggerAgent::UpdateSubtreeBreakpoints(Node* node,
                                                         uint32_t root_mask,
                                                         bool set) {
  uint32_t old_mask = FindBreakpointMask(node);
  uint32_t derived_mask = root_mask << domBreakpointDerivedTypeShift;
  uint32_t new_mask = set ? old_mask | derived_mask : old_mask & ~derived_mask;
  if (new_mask)
    dom_breakpoints_.Set(node, new_mask);
  else
    dom_breakpoints_.erase(node);

  uint32_t new_root_mask = root_mask & ~new_mask;
  if (!new_root_mask)
    return;

  InspectorDOMAgent::IncludeWhitespaceEnum include_whitespace =
      dom_agent_->IncludeWhitespace();
  for (Node* child =
           InspectorDOMAgent::InnerFirstChild(node, include_whitespace);
       child;
       child = InspectorDOMAgent::InnerNextSibling(child, include_whitespace))
    UpdateSubtreeBreakpoints(child, new_root_mask, set);
}

void InspectorDOMDebuggerAgent::PauseOnNativeEventIfNeeded(
    std::unique_ptr<protocol::DictionaryValue> event_data,
    bool synchronous) {
  if (!event_data)
    return;
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(event_data->Serialize()), &json);
  v8_inspector::StringView json_view(json.data(), json.size());
  auto listener = ToV8InspectorStringView(
      v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::EventListener);
  if (synchronous)
    v8_session_->breakProgram(listener, json_view);
  else
    v8_session_->schedulePauseOnNextStatement(listener, json_view);
}

std::unique_ptr<protocol::DictionaryValue>
InspectorDOMDebuggerAgent::PreparePauseOnNativeEventData(
    const String& event_name,
    const String& target_name) {
  bool match = event_listener_breakpoints_.Get(
                   EventListenerBreakpointKey(event_name, "*")) ||
               event_listener_breakpoints_.Get(
                   EventListenerBreakpointKey(event_name, target_name));
  if (!match)
    return nullptr;

  const String full_event_name = kListenerEventCategoryType + event_name;
  auto event_data = protocol::DictionaryValue::create();
  event_data->setString("eventName", full_event_name);
  event_data->setString("targetName", target_name);
  return event_data;
}

void InspectorDOMDebuggerAgent::CancelNativeBreakpoint() {
  v8_session_->cancelPauseOnNextStatement();
}

void InspectorDOMDebuggerAgent::Will(const probe::UserCallback& probe) {
  // Targetless callbacks are handled by InspectorEventBreakpoints
  if (!probe.event_target) {
    return;
  }
  String name = probe.name ? String(probe.name) : probe.atomic_name;
  Node* node = probe.event_target->ToNode();
  String target_name =
      node ? node->nodeName() : probe.event_target->InterfaceName();
  PauseOnNativeEventIfNeeded(PreparePauseOnNativeEventData(name, target_name),
                             /*sync*/ false);
}

void InspectorDOMDebuggerAgent::Did(const probe::UserCallback& probe) {
  CancelNativeBreakpoint();
}

protocol::Response InspectorDOMDebuggerAgent::setXHRBreakpoint(
    const String& url) {
  if (url.empty())
    pause_on_all_xhrs_.Set(true);
  else
    xhr_breakpoints_.Set(url, true);
  DidAddBreakpoint();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMDebuggerAgent::removeXHRBreakpoint(
    const String& url) {
  if (url.empty())
    pause_on_all_xhrs_.Set(false);
  else
    xhr_breakpoints_.Clear(url);
  DidRemoveBreakpoint();
  return protocol::Response::Success();
}

// Returns the breakpoint url if a match is found, or WTF::String().
String InspectorDOMDebuggerAgent::MatchXHRBreakpoints(const String& url) const {
  if (pause_on_all_xhrs_.Get())
    return WTF::g_empty_string;
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
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(event_data->Serialize()), &json);
  v8_session_->breakProgram(
      ToV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::XHR),
      v8_inspector::StringView(json.data(), json.size()));
}

void InspectorDOMDebuggerAgent::DidAddBreakpoint() {
  if (enabled_.Get())
    return;
  SetEnabled(true);
}

void InspectorDOMDebuggerAgent::DidRemoveBreakpoint() {
  if (!dom_breakpoints_.empty())
    return;
  if (!csp_violation_breakpoints_.IsEmpty())
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
  if (enabled && !enabled_.Get()) {
    instrumenting_agents_->AddInspectorDOMDebuggerAgent(this);
    dom_agent_->AddDOMListener(this);
    enabled_.Set(true);
  } else if (!enabled && enabled_.Get()) {
    instrumenting_agents_->RemoveInspectorDOMDebuggerAgent(this);
    dom_agent_->RemoveDOMListener(this);
    enabled_.Set(false);
  }
}

void InspectorDOMDebuggerAgent::DidAddDocument(Document* document) {}

void InspectorDOMDebuggerAgent::DidModifyDOMAttr(Element* element) {}

void InspectorDOMDebuggerAgent::DidCommitLoadForLocalFrame(LocalFrame*) {
  dom_breakpoints_.clear();
}

String ViolationTypeToString(const ContentSecurityPolicyViolationType type) {
  switch (type) {
    case ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation:
      return protocol::DOMDebugger::CSPViolationTypeEnum::
          TrustedtypeSinkViolation;
    case ContentSecurityPolicyViolationType::kTrustedTypesPolicyViolation:
      return protocol::DOMDebugger::CSPViolationTypeEnum::
          TrustedtypePolicyViolation;
    default:
      return WTF::g_empty_string;
  }
}

void InspectorDOMDebuggerAgent::OnContentSecurityPolicyViolation(
    const ContentSecurityPolicyViolationType violationType) {
  auto violationString = ViolationTypeToString(violationType);
  if (!csp_violation_breakpoints_.Get(violationString))
    return;

  std::unique_ptr<protocol::DictionaryValue> event_data =
      protocol::DictionaryValue::create();
  event_data->setString("violationType", violationString);
  std::vector<uint8_t> json;
  ConvertCBORToJSON(SpanFrom(event_data->Serialize()), &json);
  v8_inspector::StringView json_view(json.data(), json.size());
  auto listener = ToV8InspectorStringView(
      v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::CSPViolation);

  v8_session_->breakProgram(listener, json_view);
}

}  // namespace blink
