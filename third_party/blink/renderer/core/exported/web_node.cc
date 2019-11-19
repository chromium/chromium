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

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WebNode::WebNode() = default;

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

WebNode WebNode::ParentNode() const {
  return WebNode(const_cast<ContainerNode*>(private_->parentNode()));
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
  auto* element = DynamicTo<Element>(private_.Get());
  if (!element)
    return false;
  if (!private_->GetDocument().HaveRenderBlockingResourcesLoaded())
    return false;
  private_->GetDocument().UpdateStyleAndLayoutTreeForNode(private_.Get());
  return element->IsFocusable();
}

bool WebNode::IsContentEditable() const {
  private_->GetDocument().UpdateStyleAndLayoutTree();
  return HasEditableStyle(*private_);
}

bool WebNode::IsInsideFocusableElementOrARIAWidget() const {
  return AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *this->ConstUnwrap<Node>());
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
      ->PostTask(
          FROM_HERE,
          WTF::Bind(&Node::DispatchSimulatedClick,
                    WrapWeakPersistent(private_.Get()), nullptr, kSendNoEvents,
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

WebVector<WebElement> WebNode::QuerySelectorAll(
    const WebString& selector) const {
  if (!private_->IsContainerNode())
    return WebVector<WebElement>();
  StaticElementList* elements =
      blink::To<ContainerNode>(private_.Get())
          ->QuerySelectorAll(selector, IGNORE_EXCEPTION_FOR_TESTING);
  if (elements) {
    WebVector<WebElement> vector((size_t)elements->length());
    for (unsigned i = 0; i < elements->length(); ++i)
      vector[i] = elements->item(i);
    return vector;
  }
  return WebVector<WebElement>();
}

bool WebNode::Focused() const {
  return private_->IsFocused();
}

WebPluginContainer* WebNode::PluginContainer() const {
  return private_->GetWebPluginContainer();
}

WebNode::WebNode(Node* node) : private_(node) {}

WebNode& WebNode::operator=(Node* node) {
  private_ = node;
  return *this;
}

WebNode::operator Node*() const {
  return private_.Get();
}

}  // namespace blink
