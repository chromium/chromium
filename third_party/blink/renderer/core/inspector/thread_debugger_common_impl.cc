// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"

#include <memory>

#include "base/check.h"
#include "base/rand_util.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_token_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_all_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
namespace blink {

ThreadDebuggerCommonImpl::ThreadDebuggerCommonImpl(v8::Isolate* isolate)
    : ThreadDebugger(isolate), isolate_(isolate) {}

ThreadDebuggerCommonImpl::~ThreadDebuggerCommonImpl() = default;

// static
mojom::ConsoleMessageLevel
ThreadDebuggerCommonImpl::V8MessageLevelToMessageLevel(
    v8::Isolate::MessageErrorLevel level) {
  mojom::ConsoleMessageLevel result = mojom::ConsoleMessageLevel::kInfo;
  switch (level) {
    case v8::Isolate::kMessageDebug:
      result = mojom::ConsoleMessageLevel::kVerbose;
      break;
    case v8::Isolate::kMessageWarning:
      result = mojom::ConsoleMessageLevel::kWarning;
      break;
    case v8::Isolate::kMessageError:
      result = mojom::ConsoleMessageLevel::kError;
      break;
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
    default:
      result = mojom::ConsoleMessageLevel::kInfo;
      break;
  }
  return result;
}

void ThreadDebuggerCommonImpl::AsyncTaskScheduled(
    const StringView& operation_name,
    void* task,
    bool recurring) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskScheduled(ToV8InspectorStringView(operation_name),
                                    task, recurring);
}

void ThreadDebuggerCommonImpl::AsyncTaskCanceled(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskCanceled(task);
}

void ThreadDebuggerCommonImpl::AllAsyncTasksCanceled() {
  v8_inspector_->allAsyncTasksCanceled();
}

void ThreadDebuggerCommonImpl::AsyncTaskStarted(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskStarted(task);
}

void ThreadDebuggerCommonImpl::AsyncTaskFinished(void* task) {
  DCHECK_EQ(reinterpret_cast<intptr_t>(task) % 2, 0);
  v8_inspector_->asyncTaskFinished(task);
}

v8_inspector::V8StackTraceId ThreadDebuggerCommonImpl::StoreCurrentStackTrace(
    const StringView& description) {
  return v8_inspector_->storeCurrentStackTrace(
      ToV8InspectorStringView(description));
}

void ThreadDebuggerCommonImpl::ExternalAsyncTaskStarted(
    const v8_inspector::V8StackTraceId& parent) {
  v8_inspector_->externalAsyncTaskStarted(parent);
}

void ThreadDebuggerCommonImpl::ExternalAsyncTaskFinished(
    const v8_inspector::V8StackTraceId& parent) {
  v8_inspector_->externalAsyncTaskFinished(parent);
}

unsigned ThreadDebuggerCommonImpl::PromiseRejected(
    v8::Local<v8::Context> context,
    const String& error_message,
    v8::Local<v8::Value> exception,
    std::unique_ptr<SourceLocation> location) {
  const StringView default_message = "Uncaught (in promise)";
  String message = error_message;
  if (message.empty()) {
    message = "Uncaught (in promise)";
  } else if (message.StartsWith("Uncaught ")) {
    message = "Uncaught (in promise)" + StringView(message, 8);
  }

  ReportConsoleMessage(
      ToExecutionContext(context), mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message, location.get());
  String url = location->Url();
  return GetV8Inspector()->exceptionThrown(
      context, ToV8InspectorStringView(default_message), exception,
      ToV8InspectorStringView(message), ToV8InspectorStringView(url),
      location->LineNumber(), location->ColumnNumber(),
      location->TakeStackTrace(), location->ScriptId());
}

void ThreadDebuggerCommonImpl::PromiseRejectionRevoked(
    v8::Local<v8::Context> context,
    unsigned promise_rejection_id) {
  const String message = "Handler added to rejected promise";
  GetV8Inspector()->exceptionRevoked(context, promise_rejection_id,
                                     ToV8InspectorStringView(message));
}

// TODO(mustaq): Is it tied to a specific user action? https://crbug.com/826293
void ThreadDebuggerCommonImpl::beginUserGesture() {
  auto* window = CurrentDOMWindow(isolate_);
  LocalFrame::NotifyUserActivation(
      window ? window->GetFrame() : nullptr,
      mojom::blink::UserActivationNotificationType::kDevTools);
}

namespace {
static const char kType[] = "type";
static const char kValue[] = "value";
enum ShadowTreeSerialization { kNone, kOpen, kAll };

v8::Local<v8::String> TypeStringKey(v8::Isolate* isolate_) {
  return V8String(isolate_, kType);
}
v8::Local<v8::String> ValueStringKey(v8::Isolate* isolate_) {
  return V8String(isolate_, kValue);
}

v8::Local<v8::Object> SerializeNodeToV8Object(
    Node* node,
    v8::Isolate* isolate,
    int max_node_depth,
    ShadowTreeSerialization include_shadow_tree) {
  static const char kAttributes[] = "attributes";
  static const char kBackendNodeId[] = "backendNodeId";
  static const char kChildren[] = "children";
  static const char kChildNodeCount[] = "childNodeCount";
  static const char kLoaderId[] = "loaderId";
  static const char kLocalName[] = "localName";
  static const char kNamespaceURI[] = "namespaceURI";
  static const char kNode[] = "node";
  static const char kNodeType[] = "nodeType";
  static const char kNodeValue[] = "nodeValue";
  static const char kShadowRoot[] = "shadowRoot";
  static const char kShadowRootMode[] = "mode";
  static const char kShadowRootOpen[] = "open";
  static const char kShadowRootClosed[] = "closed";
  static const char kFrameIdParameterName[] = "frameId";

  v8::LocalVector<v8::Name> serialized_value_keys(isolate);
  v8::LocalVector<v8::Value> serialized_value_values(isolate);
  serialized_value_keys.push_back(V8String(isolate, kNodeType));
  serialized_value_values.push_back(
      v8::Number::New(isolate, node->getNodeType()));

  if (!node->nodeValue().IsNull()) {
    serialized_value_keys.push_back(V8String(isolate, kNodeValue));
    serialized_value_values.push_back(V8String(isolate, node->nodeValue()));
  }

  serialized_value_keys.push_back(V8String(isolate, kChildNodeCount));
  serialized_value_values.push_back(
      v8::Number::New(isolate, node->CountChildren()));

  DOMNodeId backend_node_id = node->GetDomNodeId();
  serialized_value_keys.push_back(V8String(isolate, kBackendNodeId));
  serialized_value_values.push_back(v8::Number::New(isolate, backend_node_id));

  serialized_value_keys.push_back(V8String(isolate, kLoaderId));
  serialized_value_values.push_back(V8String(
      isolate, IdentifiersFactory::LoaderId(node->GetDocument().Loader())));

  if (node->IsAttributeNode()) {
    Attr* attribute = To<Attr>(node);

    serialized_value_keys.push_back(V8String(isolate, kLocalName));
    serialized_value_values.push_back(
        V8String(isolate, attribute->localName()));

    serialized_value_keys.push_back(V8String(isolate, kNamespaceURI));
    if (attribute->namespaceURI().IsNull()) {
      serialized_value_values.push_back(v8::Null(isolate));
    } else {
      serialized_value_values.push_back(
          V8String(isolate, attribute->namespaceURI()));
    }
  }

  if (node->IsElementNode()) {
    Element* element = To<Element>(node);

    if (HTMLFrameOwnerElement* frameOwnerElement =
            DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (frameOwnerElement->ContentFrame()) {
        serialized_value_keys.push_back(
            V8String(isolate, kFrameIdParameterName));
        serialized_value_values.push_back(V8String(
            isolate,
            IdentifiersFactory::IdFromToken(
                frameOwnerElement->ContentFrame()->GetDevToolsFrameToken())));
      }
    }

    if (ShadowRoot* shadow_root = node->GetShadowRoot()) {
      // Do not decrease `max_node_depth` for shadow root. Shadow root should be
      // serialized fully, while it's children will be serialized respecting
      // max_node_depth and include_shadow_tree.
      v8::Local<v8::Object> serialized_shadow = SerializeNodeToV8Object(
          shadow_root, isolate, max_node_depth, include_shadow_tree);

      serialized_value_keys.push_back(V8String(isolate, kShadowRoot));
      serialized_value_values.push_back(serialized_shadow);
    } else {
      serialized_value_keys.push_back(V8String(isolate, kShadowRoot));
      serialized_value_values.push_back(v8::Null(isolate));
    }

    serialized_value_keys.push_back(V8String(isolate, kLocalName));
    serialized_value_values.push_back(V8String(isolate, element->localName()));

    serialized_value_keys.push_back(V8String(isolate, kNamespaceURI));
    serialized_value_values.push_back(
        V8String(isolate, element->namespaceURI()));

    v8::LocalVector<v8::Name> node_attributes_keys(isolate);
    v8::LocalVector<v8::Value> node_attributes_values(isolate);

    for (const Attribute& attribute : element->Attributes()) {
      node_attributes_keys.push_back(
          V8String(isolate, attribute.GetName().ToString()));
      node_attributes_values.push_back(V8String(isolate, attribute.Value()));
    }

    DCHECK(node_attributes_values.size() == node_attributes_keys.size());
    v8::Local<v8::Object> node_attributes = v8::Object::New(
        isolate, v8::Null(isolate), node_attributes_keys.data(),
        node_attributes_values.data(), node_attributes_keys.size());

    serialized_value_keys.push_back(V8String(isolate, kAttributes));
    serialized_value_values.push_back(node_attributes);
  }

  bool include_children = max_node_depth > 0;
  if (node->IsShadowRoot()) {
    ShadowRoot* shadow_root = To<ShadowRoot>(node);

    // Include children of shadow root only if `max_depth` is not reached and
    // one of the following is true:
    // 1. `include_shadow_tree` set to `all` regardless of the shadow type.
    // 2. `include_shadow_tree` set to `open` and the shadow type is `open`.
    if (include_shadow_tree == kNone) {
      include_children = false;
    } else if (include_shadow_tree == kOpen &&
               shadow_root->GetMode() != ShadowRootMode::kOpen) {
      include_children = false;
    }

    serialized_value_keys.push_back(V8String(isolate, kShadowRootMode));
    serialized_value_values.push_back(
        V8String(isolate, shadow_root->GetMode() == ShadowRootMode::kOpen
                              ? kShadowRootOpen
                              : kShadowRootClosed));
  }

  if (include_children) {
    NodeList* child_nodes = node->childNodes();

    v8::Local<v8::Array> children =
        v8::Array::New(isolate, child_nodes->length());

    for (unsigned int i = 0; i < child_nodes->length(); i++) {
      Node* child_node = child_nodes->item(i);
      v8::Local<v8::Object> serialized_child_node = SerializeNodeToV8Object(
          child_node, isolate, max_node_depth - 1, include_shadow_tree);

      children
          ->CreateDataProperty(isolate->GetCurrentContext(), i,
                               serialized_child_node)
          .Check();
    }
    serialized_value_keys.push_back(V8String(isolate, kChildren));
    serialized_value_values.push_back(children);
  }

  DCHECK(serialized_value_values.size() == serialized_value_keys.size());

  v8::Local<v8::Object> serialized_value = v8::Object::New(
      isolate, v8::Null(isolate), serialized_value_keys.data(),
      serialized_value_values.data(), serialized_value_keys.size());

  v8::LocalVector<v8::Name> result_keys(isolate);
  v8::LocalVector<v8::Value> result_values(isolate);

  result_keys.push_back(TypeStringKey(isolate));
  result_values.push_back(V8String(isolate, kNode));

  result_keys.push_back(ValueStringKey(isolate));
  result_values.push_back(serialized_value);

  return v8::Object::New(isolate, v8::Null(isolate), result_keys.data(),
                         result_values.data(), result_keys.size());
}

std::unique_ptr<v8_inspector::DeepSerializedValue> DeepSerializeHtmlCollection(
    HTMLCollection* html_collection,
    v8::Isolate* isolate_,
    int max_depth,
    int max_node_depth,
    ShadowTreeSerialization include_shadow_tree) {
  static const char kHtmlCollection[] = "htmlcollection";
  if (max_depth > 0) {
    v8::Local<v8::Array> children =
        v8::Array::New(isolate_, html_collection->length());

    for (unsigned int i = 0; i < html_collection->length(); i++) {
      Node* child_node = html_collection->item(i);
      v8::Local<v8::Object> serialized_child_node = SerializeNodeToV8Object(
          child_node, isolate_, max_node_depth, include_shadow_tree);
      children
          ->CreateDataProperty(isolate_->GetCurrentContext(), i,
                               serialized_child_node)
          .Check();
    }
    return std::make_unique<v8_inspector::DeepSerializedValue>(
        ToV8InspectorStringBuffer(kHtmlCollection), children);
  }

  return std::make_unique<v8_inspector::DeepSerializedValue>(
      ToV8InspectorStringBuffer(kHtmlCollection));
}

std::unique_ptr<v8_inspector::DeepSerializedValue> DeepSerializeNodeList(
    NodeList* node_list,
    v8::Isolate* isolate_,
    int max_depth,
    int max_node_depth,
    ShadowTreeSerialization include_shadow_tree) {
  static const char kNodeList[] = "nodelist";
  if (max_depth > 0) {
    v8::Local<v8::Array> children =
        v8::Array::New(isolate_, node_list->length());

    for (unsigned int i = 0; i < node_list->length(); i++) {
      Node* child_node = node_list->item(i);
      v8::Local<v8::Object> serialized_child_node = SerializeNodeToV8Object(
          child_node, isolate_, max_node_depth, include_shadow_tree);
      children
          ->CreateDataProperty(isolate_->GetCurrentContext(), i,
                               serialized_child_node)
          .Check();
    }
    return std::make_unique<v8_inspector::DeepSerializedValue>(
        ToV8InspectorStringBuffer(kNodeList), children);
  }

  return std::make_unique<v8_inspector::DeepSerializedValue>(
      ToV8InspectorStringBuffer(kNodeList));
}

std::unique_ptr<v8_inspector::DeepSerializedValue> DeepSerializeNode(
    Node* node,
    v8::Isolate* isolate,
    int max_node_depth,
    ShadowTreeSerialization include_shadow_tree) {
  v8::Local<v8::Object> node_v8_object = SerializeNodeToV8Object(
      node, isolate, max_node_depth, include_shadow_tree);

  v8::Local<v8::Value> value_v8_object =
      node_v8_object->Get(isolate->GetCurrentContext(), ValueStringKey(isolate))
          .ToLocalChecked();

  // Safely get `type` from object value.
  v8::MaybeLocal<v8::Value> maybe_type_v8_value =
      node_v8_object->Get(isolate->GetCurrentContext(), TypeStringKey(isolate));
  DCHECK(!maybe_type_v8_value.IsEmpty());
  v8::Local<v8::Value> type_v8_value = maybe_type_v8_value.ToLocalChecked();
  DCHECK(type_v8_value->IsString());
  v8::Local<v8::String> type_v8_string = type_v8_value.As<v8::String>();
  String type_string = ToCoreString(isolate, type_v8_string);
  StringView type_string_view = StringView(type_string);
  std::unique_ptr<v8_inspector::StringBuffer> type_string_buffer =
      ToV8InspectorStringBuffer(type_string_view);

  return std::make_unique<v8_inspector::DeepSerializedValue>(
      std::move(type_string_buffer), value_v8_object);
}

std::unique_ptr<v8_inspector::DeepSerializedValue> DeepSerializeWindow(
    DOMWindow* window,
    v8::Isolate* isolate) {
  static const char kContextParameterName[] = "context";

  v8::LocalVector<v8::Name> keys(isolate);
  v8::LocalVector<v8::Value> values(isolate);

  keys.push_back(V8String(isolate, kContextParameterName));
  values.push_back(
      V8String(isolate, IdentifiersFactory::IdFromToken(
                            window->GetFrame()->GetDevToolsFrameToken())));

  return std::make_unique<v8_inspector::DeepSerializedValue>(
      ToV8InspectorStringBuffer("window"),
      v8::Object::New(isolate, v8::Null(isolate), keys.data(), values.data(),
                      keys.size()));
}

}  // namespace

// If `additional_parameters` cannot be parsed, return `false` and provide
// `error_message`.
bool ReadAdditionalSerializationParameters(
    v8::Local<v8::Object> additional_parameters,
    int& max_node_depth,
    ShadowTreeSerialization& include_shadow_tree,
    v8::Local<v8::Context> context,
    std::unique_ptr<v8_inspector::StringBuffer>* error_message) {
  static const char kMaxNodeDepthParameterName[] = "maxNodeDepth";
  static const char kIncludeShadowTreeParameterName[] = "includeShadowTree";
  static const char kIncludeShadowTreeValueNone[] = "none";
  static const char kIncludeShadowTreeValueOpen[] = "open";
  static const char kIncludeShadowTreeValueAll[] = "all";

  // Set default values.
  max_node_depth = 0;
  include_shadow_tree = ShadowTreeSerialization::kNone;

  if (additional_parameters.IsEmpty()) {
    return true;
  }

  v8::MaybeLocal<v8::Value> include_shadow_tree_parameter =
      additional_parameters->Get(
          context,
          V8String(context->GetIsolate(), kIncludeShadowTreeParameterName));
  if (!include_shadow_tree_parameter.IsEmpty()) {
    v8::Local<v8::Value> include_shadow_tree_value =
        include_shadow_tree_parameter.ToLocalChecked();
    if (!include_shadow_tree_value->IsUndefined()) {
      if (!include_shadow_tree_value->IsString()) {
        *error_message = ToV8InspectorStringBuffer(
            String("Parameter " + String(kIncludeShadowTreeParameterName) +
                   " should be of type string."));
        return false;
      }
      String include_shadow_tree_string = ToCoreString(
          context->GetIsolate(), include_shadow_tree_value.As<v8::String>());

      if (include_shadow_tree_string == kIncludeShadowTreeValueNone) {
        include_shadow_tree = ShadowTreeSerialization::kNone;
      } else if (include_shadow_tree_string == kIncludeShadowTreeValueOpen) {
        include_shadow_tree = ShadowTreeSerialization::kOpen;
      } else if (include_shadow_tree_string == kIncludeShadowTreeValueAll) {
        include_shadow_tree = ShadowTreeSerialization::kAll;
      } else {
        *error_message = ToV8InspectorStringBuffer(
            String("Unknown value " + String(kIncludeShadowTreeParameterName) +
                   ":" + include_shadow_tree_string));
        return false;
      }
    }
  }

  v8::MaybeLocal<v8::Value> max_node_depth_parameter =
      additional_parameters->Get(
          context, V8String(context->GetIsolate(), kMaxNodeDepthParameterName));
  if (!max_node_depth_parameter.IsEmpty()) {
    v8::Local<v8::Value> max_node_depth_value =
        max_node_depth_parameter.ToLocalChecked();
    if (!max_node_depth_value->IsUndefined()) {
      if (!max_node_depth_value->IsInt32()) {
        *error_message = ToV8InspectorStringBuffer(
            String("Parameter " + String(kMaxNodeDepthParameterName) +
                   " should be of type int."));
        return false;
      }
      max_node_depth = max_node_depth_value.As<v8::Int32>()->Value();
    }
  }
  return true;
}

std::unique_ptr<v8_inspector::DeepSerializationResult>
ThreadDebuggerCommonImpl::deepSerialize(
    v8::Local<v8::Value> v8_value,
    int max_depth,
    v8::Local<v8::Object> additional_parameters) {
  int max_node_depth;
  ShadowTreeSerialization include_shadow_tree;

  std::unique_ptr<v8_inspector::StringBuffer> error_message;
  bool success = ReadAdditionalSerializationParameters(
      additional_parameters, max_node_depth, include_shadow_tree,
      isolate_->GetCurrentContext(), &error_message);
  if (!success) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        std::move(error_message));
  }

  if (!v8_value->IsObject()) {
    return nullptr;
  }
  v8::Local<v8::Object> object = v8_value.As<v8::Object>();

  // Serialize according to https://w3c.github.io/webdriver-bidi.
  if (Node* node = V8Node::ToWrappable(isolate_, object)) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        DeepSerializeNode(node, isolate_, max_node_depth, include_shadow_tree));
  }

  // Serialize as a regular array
  if (HTMLCollection* html_collection =
          V8HTMLCollection::ToWrappable(isolate_, object)) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        DeepSerializeHtmlCollection(html_collection, isolate_, max_depth,
                                    max_node_depth, include_shadow_tree));
  }

  // Serialize as a regular array
  if (NodeList* node_list = V8NodeList::ToWrappable(isolate_, object)) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        DeepSerializeNodeList(node_list, isolate_, max_depth, max_node_depth,
                              include_shadow_tree));
  }

  if (DOMWindow* window = V8Window::ToWrappable(isolate_, object)) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        DeepSerializeWindow(window, isolate_));
  }

  // TODO(caseq): consider object->IsApiWrapper() + checking for all kinds
  // of (Typed)?Array(Buffers)?. IsApiWrapper() returns true for these, but
  // we want them to fall through to default serialization and not be treated
  // as "platform objects".
  if (V8DOMWrapper::IsWrapper(isolate_, object)) {
    return std::make_unique<v8_inspector::DeepSerializationResult>(
        std::make_unique<v8_inspector::DeepSerializedValue>(
            ToV8InspectorStringBuffer("platformobject")));
  }

  return nullptr;
}

std::unique_ptr<v8_inspector::StringBuffer>
ThreadDebuggerCommonImpl::valueSubtype(v8::Local<v8::Value> value) {
  static const char kNode[] = "node";
  static const char kArray[] = "array";
  static const char kError[] = "error";
  static const char kBlob[] = "blob";
  static const char kTrustedType[] = "trustedtype";

  if (V8Node::HasInstance(isolate_, value)) {
    return ToV8InspectorStringBuffer(kNode);
  }
  if (V8NodeList::HasInstance(isolate_, value) ||
      V8DOMTokenList::HasInstance(isolate_, value) ||
      V8HTMLCollection::HasInstance(isolate_, value) ||
      V8HTMLAllCollection::HasInstance(isolate_, value)) {
    return ToV8InspectorStringBuffer(kArray);
  }
  if (V8DOMException::HasInstance(isolate_, value)) {
    return ToV8InspectorStringBuffer(kError);
  }
  if (V8Blob::HasInstance(isolate_, value)) {
    return ToV8InspectorStringBuffer(kBlob);
  }
  if (V8TrustedHTML::HasInstance(isolate_, value) ||
      V8TrustedScript::HasInstance(isolate_, value) ||
      V8TrustedScriptURL::HasInstance(isolate_, value)) {
    return ToV8InspectorStringBuffer(kTrustedType);
  }
  return nullptr;
}

std::unique_ptr<v8_inspector::StringBuffer>
ThreadDebuggerCommonImpl::descriptionForValueSubtype(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value) {
  if (TrustedHTML* trusted_html = V8TrustedHTML::ToWrappable(isolate_, value)) {
    return ToV8InspectorStringBuffer(trusted_html->toString());
  } else if (TrustedScript* trusted_script =
                 V8TrustedScript::ToWrappable(isolate_, value)) {
    return ToV8InspectorStringBuffer(trusted_script->toString());
  } else if (TrustedScriptURL* trusted_script_url =
                 V8TrustedScriptURL::ToWrappable(isolate_, value)) {
    return ToV8InspectorStringBuffer(trusted_script_url->toString());
  } else if (Node* node = V8Node::ToWrappable(isolate_, value)) {
    StringBuilder description;
    switch (node->getNodeType()) {
      case Node::kElementNode: {
        const auto* element = To<blink::Element>(node);
        description.Append(element->TagQName().ToString());

        const AtomicString& id = element->GetIdAttribute();
        if (!id.empty()) {
          description.Append('#');
          description.Append(id);
        }
        if (element->HasClass()) {
          auto element_class_names = element->ClassNames();
          auto n_classes = element_class_names.size();
          for (unsigned i = 0; i < n_classes; ++i) {
            description.Append('.');
            description.Append(element_class_names[i]);
          }
        }
        break;
      }
      case Node::kDocumentTypeNode: {
        description.Append("<!DOCTYPE ");
        description.Append(node->nodeName());
        description.Append('>');
        break;
      }
      default: {
        description.Append(node->nodeName());
        break;
      }
    }
    DCHECK(description.length());

    return ToV8InspectorStringBuffer(description.ToString());
  }
  return nullptr;
}

double ThreadDebuggerCommonImpl::currentTimeMS() {
  return base::Time::Now().InMillisecondsFSinceUnixEpoch();
}

bool ThreadDebuggerCommonImpl::isInspectableHeapObject(
    v8::Local<v8::Object> object) {
  return !object->IsApiWrapper() || V8DOMWrapper::IsWrapper(isolate_, object);
}

static void ReturnDataCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.Data());
}

static v8::Maybe<bool> CreateDataProperty(v8::Local<v8::Context> context,
                                          v8::Local<v8::Object> object,
                                          v8::Local<v8::Name> key,
                                          v8::Local<v8::Value> value) {
  v8::TryCatch try_catch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throw_js(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return object->CreateDataProperty(context, key, value);
}

static void CreateFunctionPropertyWithData(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* name,
    v8::FunctionCallback callback,
    v8::Local<v8::Value> data,
    const char* description,
    v8::SideEffectType side_effect_type) {
  v8::Local<v8::String> func_name = V8String(context->GetIsolate(), name);
  v8::Local<v8::Function> func;
  if (!v8::Function::New(context, callback, data, 0,
                         v8::ConstructorBehavior::kThrow, side_effect_type)
           .ToLocal(&func))
    return;
  func->SetName(func_name);
  v8::Local<v8::String> return_value =
      V8String(context->GetIsolate(), description);
  v8::Local<v8::Function> to_string_function;
  if (v8::Function::New(context, ReturnDataCallback, return_value, 0,
                        v8::ConstructorBehavior::kThrow,
                        v8::SideEffectType::kHasNoSideEffect)
          .ToLocal(&to_string_function))
    CreateDataProperty(context, func,
                       V8AtomicString(context->GetIsolate(), "toString"),
                       to_string_function);
  CreateDataProperty(context, object, func_name, func);
}

v8::Maybe<bool> ThreadDebuggerCommonImpl::CreateDataPropertyInArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    int index,
    v8::Local<v8::Value> value) {
  v8::TryCatch try_catch(context->GetIsolate());
  v8::Isolate::DisallowJavascriptExecutionScope throw_js(
      context->GetIsolate(),
      v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return array->CreateDataProperty(context, index, value);
}

void ThreadDebuggerCommonImpl::CreateFunctionProperty(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* name,
    v8::FunctionCallback callback,
    const char* description,
    v8::SideEffectType side_effect_type) {
  CreateFunctionPropertyWithData(context, object, name, callback,
                                 v8::External::New(context->GetIsolate(), this),
                                 description, side_effect_type);
}

void ThreadDebuggerCommonImpl::installAdditionalCommandLineAPI(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object) {
  CreateFunctionProperty(
      context, object, "getEventListeners",
      ThreadDebuggerCommonImpl::GetEventListenersCallback,
      "function getEventListeners(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  CreateFunctionProperty(
      context, object, "getAccessibleName",
      ThreadDebuggerCommonImpl::GetAccessibleNameCallback,
      "function getAccessibleName(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  CreateFunctionProperty(
      context, object, "getAccessibleRole",
      ThreadDebuggerCommonImpl::GetAccessibleRoleCallback,
      "function getAccessibleRole(node) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);

  v8::Isolate* isolate = context->GetIsolate();
  ScriptEvaluationResult result =
      ClassicScript::CreateUnspecifiedScript(
          "(function(e) { console.log(e.type, e); })",
          ScriptSourceLocationType::kInternal)
          ->RunScriptOnScriptStateAndReturnValue(
              ScriptState::From(isolate, context));
  if (result.GetResultType() != ScriptEvaluationResult::ResultType::kSuccess) {
    // On pages where scripting is disabled or CSP sandbox directive is used,
    // this can be blocked and thus early exited here.
    // This is probably OK because `monitorEvents()` console API is anyway not
    // working on such pages. For more discussion see
    // https://crrev.com/c/3258735/9/third_party/blink/renderer/core/inspector/thread_debugger.cc#529
    return;
  }

  v8::Local<v8::Value> function_value = result.GetSuccessValue();
  DCHECK(function_value->IsFunction());
  CreateFunctionPropertyWithData(
      context, object, "monitorEvents",
      ThreadDebuggerCommonImpl::MonitorEventsCallback, function_value,
      "function monitorEvents(object, [types]) { [Command Line API] }",
      v8::SideEffectType::kHasSideEffect);
  CreateFunctionPropertyWithData(
      context, object, "unmonitorEvents",
      ThreadDebuggerCommonImpl::UnmonitorEventsCallback, function_value,
      "function unmonitorEvents(object, [types]) { [Command Line API] }",
      v8::SideEffectType::kHasSideEffect);
}

static Vector<String> NormalizeEventTypes(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Vector<String> types;
  v8::Isolate* isolate = info.GetIsolate();
  if (info.Length() > 1 && info[1]->IsString())
    types.push_back(ToCoreString(isolate, info[1].As<v8::String>()));
  if (info.Length() > 1 && info[1]->IsArray()) {
    v8::Local<v8::Array> types_array = v8::Local<v8::Array>::Cast(info[1]);
    for (wtf_size_t i = 0; i < types_array->Length(); ++i) {
      v8::Local<v8::Value> type_value;
      if (!types_array->Get(isolate->GetCurrentContext(), i)
               .ToLocal(&type_value) ||
          !type_value->IsString()) {
        continue;
      }
      types.push_back(
          ToCoreString(isolate, v8::Local<v8::String>::Cast(type_value)));
    }
  }
  if (info.Length() == 1)
    types.AppendVector(
        Vector<String>({"mouse",   "key",          "touch",
                        "pointer", "control",      "load",
                        "unload",  "abort",        "error",
                        "select",  "input",        "change",
                        "submit",  "reset",        "focus",
                        "blur",    "resize",       "scroll",
                        "search",  "devicemotion", "deviceorientation"}));

  Vector<String> output_types;
  for (wtf_size_t i = 0; i < types.size(); ++i) {
    if (types[i] == "mouse")
      output_types.AppendVector(
          Vector<String>({"auxclick", "click", "dblclick", "mousedown",
                          "mouseeenter", "mouseleave", "mousemove", "mouseout",
                          "mouseover", "mouseup", "mouseleave", "mousewheel"}));
    else if (types[i] == "key")
      output_types.AppendVector(
          Vector<String>({"keydown", "keyup", "keypress", "textInput"}));
    else if (types[i] == "touch")
      output_types.AppendVector(Vector<String>(
          {"touchstart", "touchmove", "touchend", "touchcancel"}));
    else if (types[i] == "pointer")
      output_types.AppendVector(Vector<String>(
          {"pointerover", "pointerout", "pointerenter", "pointerleave",
           "pointerdown", "pointerup", "pointermove", "pointercancel",
           "gotpointercapture", "lostpointercapture"}));
    else if (types[i] == "control")
      output_types.AppendVector(
          Vector<String>({"resize", "scroll", "zoom", "focus", "blur", "select",
                          "input", "change", "submit", "reset"}));
    else
      output_types.push_back(types[i]);
  }
  return output_types;
}

static EventTarget* FirstArgumentAsEventTarget(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return nullptr;
  return V8EventTarget::ToWrappable(info.GetIsolate(), info[0]);
}

void ThreadDebuggerCommonImpl::SetMonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    bool enabled) {
  EventTarget* event_target = FirstArgumentAsEventTarget(info);
  if (!event_target)
    return;
  Vector<String> types = NormalizeEventTypes(info);
  DCHECK(!info.Data().IsEmpty() && info.Data()->IsFunction());
  V8EventListener* event_listener =
      V8EventListener::Create(info.Data().As<v8::Function>());
  for (wtf_size_t i = 0; i < types.size(); ++i) {
    if (enabled)
      event_target->addEventListener(AtomicString(types[i]), event_listener);
    else
      event_target->removeEventListener(AtomicString(types[i]), event_listener);
  }
}

// static
void ThreadDebuggerCommonImpl::MonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SetMonitorEventsCallback(info, true);
}

// static
void ThreadDebuggerCommonImpl::UnmonitorEventsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SetMonitorEventsCallback(info, false);
}

// static
void ThreadDebuggerCommonImpl::GetAccessibleNameCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value = info[0];

  Node* node = V8Node::ToWrappable(isolate, value);
  if (node && !node->GetLayoutObject())
    return;
  if (auto* element = DynamicTo<Element>(node)) {
    bindings::V8SetReturnValue(info, element->computedName(), isolate,
                               bindings::V8ReturnValue::kNonNullable);
  }
}

// static
void ThreadDebuggerCommonImpl::GetAccessibleRoleCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value = info[0];

  Node* node = V8Node::ToWrappable(isolate, value);
  if (node && !node->GetLayoutObject())
    return;
  if (auto* element = DynamicTo<Element>(node)) {
    bindings::V8SetReturnValue(info, element->computedRole(), isolate,
                               bindings::V8ReturnValue::kNonNullable);
  }
}

// static
void ThreadDebuggerCommonImpl::GetEventListenersCallback(
    const v8::FunctionCallbackInfo<v8::Value>& callback_info) {
  if (callback_info.Length() < 1)
    return;

  ThreadDebuggerCommonImpl* debugger = static_cast<ThreadDebuggerCommonImpl*>(
      v8::Local<v8::External>::Cast(callback_info.Data())->Value());
  DCHECK(debugger);
  v8::Isolate* isolate = callback_info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  int group_id = debugger->ContextGroupId(ToExecutionContext(context));

  V8EventListenerInfoList listener_info;
  // eventListeners call can produce message on ErrorEvent during lazy event
  // listener compilation.
  if (group_id)
    debugger->muteMetrics(group_id);
  InspectorDOMDebuggerAgent::EventListenersInfoForTarget(
      isolate, callback_info[0], &listener_info);
  if (group_id)
    debugger->unmuteMetrics(group_id);

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  AtomicString current_event_type;
  v8::Local<v8::Array> listeners;
  wtf_size_t output_index = 0;
  for (auto& info : listener_info) {
    if (current_event_type != info.event_type) {
      current_event_type = info.event_type;
      listeners = v8::Array::New(isolate);
      output_index = 0;
      CreateDataProperty(context, result,
                         V8AtomicString(isolate, current_event_type),
                         listeners);
    }

    v8::Local<v8::Object> listener_object = v8::Object::New(isolate);
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "listener"), info.handler);
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "useCapture"),
                       v8::Boolean::New(isolate, info.use_capture));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "passive"),
                       v8::Boolean::New(isolate, info.passive));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "once"),
                       v8::Boolean::New(isolate, info.once));
    CreateDataProperty(context, listener_object,
                       V8AtomicString(isolate, "type"),
                       V8String(isolate, current_event_type));
    CreateDataPropertyInArray(context, listeners, output_index++,
                              listener_object);
  }
  callback_info.GetReturnValue().Set(result);
}

static uint64_t GetTraceId(ThreadDebuggerCommonImpl* this_thread_debugger,
                           v8::Local<v8::String> label) {
  unsigned label_hash = label->GetIdentityHash();
  return label_hash ^ (reinterpret_cast<uintptr_t>(this_thread_debugger));
}

void ThreadDebuggerCommonImpl::consoleTime(v8::Isolate* isolate,
                                           v8::Local<v8::String> label) {
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN0(
      "blink.console", ToCoreString(isolate, label).Utf8().c_str(),
      TRACE_ID_WITH_SCOPE("console.time",
                          TRACE_ID_LOCAL(GetTraceId(this, label))));
}

void ThreadDebuggerCommonImpl::consoleTimeEnd(v8::Isolate* isolate,
                                              v8::Local<v8::String> label) {
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END0(
      "blink.console", ToCoreString(isolate, label).Utf8().c_str(),
      TRACE_ID_WITH_SCOPE("console.time",
                          TRACE_ID_LOCAL(GetTraceId(this, label))));
}

void ThreadDebuggerCommonImpl::consoleTimeStamp(v8::Isolate* isolate,
                                                v8::Local<v8::String> label) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "TimeStamp", inspector_time_stamp_event::Data,
      CurrentExecutionContext(isolate_), ToCoreString(isolate, label));
  probe::ConsoleTimeStamp(isolate_, label);
}

void ThreadDebuggerCommonImpl::startRepeatingTimer(
    double interval,
    V8InspectorClient::TimerCallback callback,
    void* data) {
  timer_data_.push_back(data);
  timer_callbacks_.push_back(callback);

  std::unique_ptr<TaskRunnerTimer<ThreadDebuggerCommonImpl>> timer =
      std::make_unique<TaskRunnerTimer<ThreadDebuggerCommonImpl>>(
          ThreadScheduler::Current()->V8TaskRunner(), this,
          &ThreadDebuggerCommonImpl::OnTimer);
  TaskRunnerTimer<ThreadDebuggerCommonImpl>* timer_ptr = timer.get();
  timers_.push_back(std::move(timer));
  timer_ptr->StartRepeating(base::Seconds(interval), FROM_HERE);
}

void ThreadDebuggerCommonImpl::cancelTimer(void* data) {
  for (wtf_size_t index = 0; index < timer_data_.size(); ++index) {
    if (timer_data_[index] == data) {
      timers_[index]->Stop();
      timer_callbacks_.EraseAt(index);
      timers_.EraseAt(index);
      timer_data_.EraseAt(index);
      return;
    }
  }
}

int64_t ThreadDebuggerCommonImpl::generateUniqueId() {
  int64_t result;
  base::RandBytes(base::byte_span_from_ref(result));
  return result;
}

void ThreadDebuggerCommonImpl::OnTimer(TimerBase* timer) {
  for (wtf_size_t index = 0; index < timers_.size(); ++index) {
    if (timers_[index].get() == timer) {
      timer_callbacks_[index](timer_data_[index]);
      return;
    }
  }
}

}  // namespace blink
