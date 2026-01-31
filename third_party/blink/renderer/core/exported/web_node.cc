/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_node.h"

#include <ostream>

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

const AtomicString& GetEventTypeName(WebNode::EventType event_type) {
  switch (event_type) {
    case WebNode::EventType::kAutofill:
      return event_type_names::kAutofill;
    case WebNode::EventType::kSelectionchange:
      return event_type_names::kSelectionchange;
    case WebNode::EventType::kBeforeinput:
      return event_type_names::kBeforeinput;
    case WebNode::EventType::kInput:
      return event_type_names::kInput;
    case WebNode::EventType::kCompositionstart:
      return event_type_names::kCompositionstart;
    case WebNode::EventType::kCompositionupdate:
      return event_type_names::kCompositionupdate;
    case WebNode::EventType::kCompositionend:
      return event_type_names::kCompositionend;
    case WebNode::EventType::kDrop:
      return event_type_names::kDrop;
    case WebNode::EventType::kPaste:
      return event_type_names::kPaste;
    case WebNode::EventType::kKeydown:
      return event_type_names::kKeydown;
    case WebNode::EventType::kKeyup:
      return event_type_names::kKeyup;
    case WebNode::EventType::kKeypress:
      return event_type_names::kKeypress;
  }
  NOTREACHED();
}

}  // namespace

WebNode::WebNode(cppgc::SourceLocation loc) : private_(loc) {}

WebNode::WebNode(const WebNode& n) {
  Assign(n);
}

WebNode& WebNode::operator=(const WebNode& n) {
  Assign(n);
  return *this;
}

WebNode::~WebNode() {
  Reset();
}

void WebNode::Reset() {
  private_.Reset();
}

void WebNode::Assign(const WebNode& other) {
  private_ = other.private_;
}

bool WebNode::Equals(const WebNode& n) const {
  return private_.Get() == n.private_.Get();
}

bool WebNode::LessThan(const WebNode& n) const {
  return private_.Get() < n.private_.Get();
}

bool WebNode::Contains(const WebNode* n) const {
  return private_->contains(n->private_.Get());
}

bool WebNode::ContainsViaFlatTree(const WebNode* n) const {
  return private_->ContainsViaFlatTree(*n->private_.Get());
}

WebNode WebNode::ParentNode() const {
  return WebNode(const_cast<ContainerNode*>(private_->parentNode()));
}

WebNode WebNode::ParentOrShadowHostNode() const {
  return WebNode(
      const_cast<ContainerNode*>(private_->ParentOrShadowHostNode()));
}

bool WebNode::IsInUserAgentShadowRoot() const {
  return private_->IsInUserAgentShadowRoot();
}

WebString WebNode::NodeValue() const {
  return private_->nodeValue();
}

WebDocument WebNode::GetDocument() const {
  return WebDocument(&private_->GetDocument());
}

WebNode WebNode::FirstChild() const {
  return WebNode(private_->firstChild());
}

WebNode WebNode::LastChild() const {
  return WebNode(private_->lastChild());
}

WebNode WebNode::PreviousSibling() const {
  return WebNode(private_->previousSibling());
}

WebNode WebNode::NextSibling() const {
  return WebNode(private_->nextSibling());
}

bool WebNode::IsNull() const {
  return private_.IsNull();
}

bool WebNode::IsConnected() const {
  return private_->isConnected();
}

bool WebNode::IsLink() const {
  return private_->IsLink();
}

bool WebNode::IsTextNode() const {
  return private_->IsTextNode();
}

bool WebNode::IsCommentNode() const {
  return private_->getNodeType() == Node::kCommentNode;
}

bool WebNode::IsFocusable() const {
  auto* element = ::blink::DynamicTo<Element>(private_.Get());
  if (!element)
    return false;
  if (!private_->GetDocument().HaveRenderBlockingResourcesLoaded())
    return false;
  // Element::IsFocusable will internally update style and layout, so there's no
  // need to do so before calling it here.
  return element->IsFocusable();
}

bool WebNode::IsContentEditable() const {
  private_->GetDocument().UpdateStyleAndLayoutTree();
  return blink::IsEditable(*private_);
}

WebElement WebNode::RootEditableElement() const {
  return blink::RootEditableElement(*private_);
}

bool WebNode::IsInsideFocusableElementOrARIAWidget() const {
  return AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *this->ConstUnwrap<Node>());
}

v8::Local<v8::Value> WebNode::ToV8Value(v8::Isolate* isolate) {
  if (!private_.Get())
    return v8::Local<v8::Value>();
  return ToV8Traits<Node>::ToV8(ScriptState::ForCurrentRealm(isolate),
                                private_.Get());
}

bool WebNode::IsElementNode() const {
  return private_->IsElementNode();
}

bool WebNode::IsDocumentNode() const {
  return private_->IsDocumentNode();
}

bool WebNode::IsDocumentTypeNode() const {
  return private_->getNodeType() == Node::kDocumentTypeNode;
}

void WebNode::SimulateClick() {
  private_->GetExecutionContext()
      ->GetTaskRunner(TaskType::kUserInteraction)
      ->PostTask(FROM_HERE,
                 BindOnce(&Node::DispatchSimulatedClick,
                          WrapWeakPersistent(private_.Get()), nullptr,
                          SimulatedClickCreationScope::kFromUserAgent));
}

WebElementCollection WebNode::GetElementsByHTMLTagName(
    const WebString& tag) const {
  if (private_->IsContainerNode()) {
    return WebElementCollection(
        blink::To<ContainerNode>(private_.Get())
            ->getElementsByTagNameNS(html_names::xhtmlNamespaceURI, tag));
  }
  return WebElementCollection();
}

WebElement WebNode::QuerySelector(const WebString& selector) const {
  if (!private_->IsContainerNode())
    return WebElement();
  return blink::To<ContainerNode>(private_.Get())
      ->QuerySelector(selector, IGNORE_EXCEPTION_FOR_TESTING);
}

std::vector<WebElement> WebNode::QuerySelectorAll(
    const WebString& selector) const {
  if (!private_->IsContainerNode())
    return std::vector<WebElement>();
  StaticElementList* elements =
      blink::To<ContainerNode>(private_.Get())
          ->QuerySelectorAll(selector, IGNORE_EXCEPTION_FOR_TESTING);
  if (elements) {
    std::vector<WebElement> vector;
    vector.reserve(elements->length());
    for (unsigned i = 0; i < elements->length(); ++i) {
      vector.push_back(elements->item(i));
    }
    return vector;
  }
  return std::vector<WebElement>();
}

std::vector<WebNode> WebNode::FindAllTextNodesMatchingRegex(
    const WebString& regex) const {
  ContainerNode* container_node =
      blink::DynamicTo<ContainerNode>(private_.Get());
  if (!container_node) {
    return std::vector<WebNode>();
  }

  StaticNodeList* nodes = container_node->FindAllTextNodesMatchingRegex(regex);
  if (!nodes) {
    return std::vector<WebNode>();
  }

  std::vector<WebNode> nodes_vector;
  nodes_vector.reserve(nodes->length());
  for (unsigned i = 0; i < nodes->length(); i++) {
    nodes_vector.push_back(nodes->item(i));
  }

  return nodes_vector;
}

bool WebNode::Focused() const {
  return private_->IsFocused();
}

void WebNode::RevealAutoExpandableAncestors() const {
  auto result = DisplayLockUtilities::RevealAutoExpandableAncestors(*private_);
  if (result.revealed_details || result.revealed_hidden_until_found) {
    private_->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);
  }
}

cc::ElementId WebNode::ScrollingElementIdForTesting() const {
  return private_->GetLayoutBox()->GetScrollableArea()->GetScrollElementId();
}

WebPluginContainer* WebNode::PluginContainer() const {
  return private_->GetWebPluginContainer();
}

WebNode::WebNode(Node* node) : private_(node) {
  DCHECK(IsMainThread());
}

WebNode& WebNode::operator=(Node* node) {
  private_ = node;
  return *this;
}

WebNode::operator Node*() const {
  return private_.Get();
}

int WebNode::GetDomNodeId() const {
  return private_.Get()->GetDomNodeId();
}

// static
WebNode WebNode::FromDomNodeId(int dom_node_id) {
  return WebNode(Node::FromDomNodeId(dom_node_id));
}

base::ScopedClosureRunner WebNode::AddEventListener(
    EventType event_type,
    base::RepeatingCallback<void(WebDOMEvent)> handler,
    bool use_capture) {
  class EventListener : public NativeEventListener {
   public:
    EventListener(Node* node,
                  EventType event_type,
                  base::RepeatingCallback<void(WebDOMEvent)> handler,
                  bool use_capture)
        : node_(node),
          event_type_(event_type),
          handler_(std::move(handler)),
          use_capture_(use_capture) {}

    void Invoke(ExecutionContext*, Event* event) override {
      handler_.Run(WebDOMEvent(event));
    }

    void AddListener() {
      node_->addEventListener(GetEventTypeName(event_type_), this,
                              /*use_capture=*/use_capture_);
    }

    void RemoveListener() {
      node_->removeEventListener(GetEventTypeName(event_type_), this,
                                 /*use_capture=*/use_capture_);
    }

    void Trace(Visitor* visitor) const override {
      NativeEventListener::Trace(visitor);
      visitor->Trace(node_);
    }

   private:
    Member<Node> node_;
    EventType event_type_;
    base::RepeatingCallback<void(WebDOMEvent)> handler_;
    bool use_capture_ = false;
  };

  WebPrivatePtrForGC<EventListener> listener =
      MakeGarbageCollected<EventListener>(Unwrap<Node>(), event_type,
                                          std::move(handler),
                                          /*use_capture=*/use_capture);
  listener->AddListener();
  return base::ScopedClosureRunner(BindOnce(
      &EventListener::RemoveListener, WrapWeakPersistent(listener.Get())));
}

bool WebNode::HasEventListeners(EventType event_type) const {
  return private_->HasEventListeners(GetEventTypeName(event_type));
}

std::ostream& operator<<(std::ostream& ostream, const WebNode& node) {
  return ostream << node.ConstUnwrap<Node>();
}

}  // namespace blink
