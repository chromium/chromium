/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/inspector/dom_editor.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/inspector/dom_patch_support.h"
#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class DOMEditor::RemoveChildAction final : public InspectorHistory::Action {
 public:
  RemoveChildAction(ContainerNode* parent_node, Node* node)
      : InspectorHistory::Action("RemoveChild"),
        parent_node_(parent_node),
        node_(node) {}
  RemoveChildAction(const RemoveChildAction&) = delete;
  RemoveChildAction& operator=(const RemoveChildAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    anchor_node_ = node_->nextSibling();
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    parent_node_->InsertBefore(node_.Get(), anchor_node_.Get(),
                               exception_state);
    return !exception_state.HadException();
  }

  bool Redo(ExceptionState& exception_state) override {
    parent_node_->RemoveChild(node_.Get(), exception_state);
    return !exception_state.HadException();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parent_node_);
    visitor->Trace(node_);
    visitor->Trace(anchor_node_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<ContainerNode> parent_node_;
  Member<Node> node_;
  Member<Node> anchor_node_;
};

class DOMEditor::InsertBeforeAction final : public InspectorHistory::Action {
 public:
  InsertBeforeAction(ContainerNode* parent_node, Node* node, Node* anchor_node)
      : InspectorHistory::Action("InsertBefore"),
        parent_node_(parent_node),
        node_(node),
        anchor_node_(anchor_node) {}
  InsertBeforeAction(const InsertBeforeAction&) = delete;
  InsertBeforeAction& operator=(const InsertBeforeAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    if (node_->parentNode()) {
      remove_child_action_ = MakeGarbageCollected<RemoveChildAction>(
          node_->parentNode(), node_.Get());
      if (!remove_child_action_->Perform(exception_state))
        return false;
    }
    parent_node_->InsertBefore(node_.Get(), anchor_node_.Get(),
                               exception_state);
    return !exception_state.HadException();
  }

  bool Undo(ExceptionState& exception_state) override {
    parent_node_->RemoveChild(node_.Get(), exception_state);
    if (exception_state.HadException())
      return false;
    if (remove_child_action_)
      return remove_child_action_->Undo(exception_state);
    return true;
  }

  bool Redo(ExceptionState& exception_state) override {
    if (remove_child_action_ && !remove_child_action_->Redo(exception_state))
      return false;
    parent_node_->InsertBefore(node_.Get(), anchor_node_.Get(),
                               exception_state);
    return !exception_state.HadException();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parent_node_);
    visitor->Trace(node_);
    visitor->Trace(anchor_node_);
    visitor->Trace(remove_child_action_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<ContainerNode> parent_node_;
  Member<Node> node_;
  Member<Node> anchor_node_;
  Member<RemoveChildAction> remove_child_action_;
};

class DOMEditor::RemoveAttributeAction final : public InspectorHistory::Action {
 public:
  RemoveAttributeAction(Element* element, const AtomicString& name)
      : InspectorHistory::Action("RemoveAttribute"),
        element_(element),
        name_(name) {}
  RemoveAttributeAction(const RemoveAttributeAction&) = delete;
  RemoveAttributeAction& operator=(const RemoveAttributeAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    value_ = element_->getAttribute(name_);
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    element_->setAttribute(name_, value_, exception_state);
    return true;
  }

  bool Redo(ExceptionState&) override {
    element_->removeAttribute(name_);
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<Element> element_;
  AtomicString name_;
  AtomicString value_;
};

class DOMEditor::SetAttributeAction final : public InspectorHistory::Action {
 public:
  SetAttributeAction(Element* element,
                     const AtomicString& name,
                     const AtomicString& value)
      : InspectorHistory::Action("SetAttribute"),
        element_(element),
        name_(name),
        value_(value),
        had_attribute_(false) {}
  SetAttributeAction(const SetAttributeAction&) = delete;
  SetAttributeAction& operator=(const SetAttributeAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    const AtomicString& value = element_->getAttribute(name_);
    had_attribute_ = !value.IsNull();
    if (had_attribute_)
      old_value_ = value;
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    if (had_attribute_)
      element_->setAttribute(name_, old_value_, exception_state);
    else
      element_->removeAttribute(name_);
    return true;
  }

  bool Redo(ExceptionState& exception_state) override {
    element_->setAttribute(name_, value_, exception_state);
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<Element> element_;
  AtomicString name_;
  AtomicString value_;
  bool had_attribute_;
  AtomicString old_value_;
};

class DOMEditor::SetOuterHTMLAction final : public InspectorHistory::Action {
 public:
  SetOuterHTMLAction(Node* node, const String& html)
      : InspectorHistory::Action("SetOuterHTML"),
        node_(node),
        next_sibling_(node->nextSibling()),
        html_(html),
        new_node_(nullptr),
        history_(MakeGarbageCollected<InspectorHistory>()),
        dom_editor_(MakeGarbageCollected<DOMEditor>(history_.Get())) {}
  SetOuterHTMLAction(const SetOuterHTMLAction&) = delete;
  SetOuterHTMLAction& operator=(const SetOuterHTMLAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    old_html_ = CreateMarkup(node_.Get());
    Document* document = IsA<Document>(node_.Get()) ? To<Document>(node_.Get())
                                                    : node_->ownerDocument();
    DCHECK(document);
    if (!document->documentElement())
      return false;
    DOMPatchSupport dom_patch_support(dom_editor_.Get(), *document);
    new_node_ =
        dom_patch_support.PatchNode(node_.Get(), html_, exception_state);
    return !exception_state.HadException();
  }

  bool Undo(ExceptionState& exception_state) override {
    return history_->Undo(exception_state);
  }

  bool Redo(ExceptionState& exception_state) override {
    return history_->Redo(exception_state);
  }

  Node* NewNode() { return new_node_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(node_);
    visitor->Trace(next_sibling_);
    visitor->Trace(new_node_);
    visitor->Trace(history_);
    visitor->Trace(dom_editor_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<Node> node_;
  Member<Node> next_sibling_;
  String html_;
  String old_html_;
  Member<Node> new_node_;
  Member<InspectorHistory> history_;
  Member<DOMEditor> dom_editor_;
};

class DOMEditor::ReplaceChildNodeAction final
    : public InspectorHistory::Action {
 public:
  ReplaceChildNodeAction(ContainerNode* parent_node,
                         Node* new_node,
                         Node* old_node)
      : InspectorHistory::Action("ReplaceChildNode"),
        parent_node_(parent_node),
        new_node_(new_node),
        old_node_(old_node) {}
  ReplaceChildNodeAction(const ReplaceChildNodeAction&) = delete;
  ReplaceChildNodeAction& operator=(const ReplaceChildNodeAction&) = delete;

  bool Perform(ExceptionState& exception_state) override {
    return Redo(exception_state);
  }

  bool Undo(ExceptionState& exception_state) override {
    parent_node_->ReplaceChild(old_node_, new_node_.Get(), exception_state);
    return !exception_state.HadException();
  }

  bool Redo(ExceptionState& exception_state) override {
    parent_node_->ReplaceChild(new_node_, old_node_.Get(), exception_state);
    return !exception_state.HadException();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parent_node_);
    visitor->Trace(new_node_);
    visitor->Trace(old_node_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<ContainerNode> parent_node_;
  Member<Node> new_node_;
  Member<Node> old_node_;
};

class DOMEditor::SetNodeValueAction final : public InspectorHistory::Action {
 public:
  SetNodeValueAction(Node* node, const String& value)
      : InspectorHistory::Action("SetNodeValue"), node_(node), value_(value) {}
  SetNodeValueAction(const SetNodeValueAction&) = delete;
  SetNodeValueAction& operator=(const SetNodeValueAction&) = delete;

  bool Perform(ExceptionState&) override {
    old_value_ = node_->nodeValue();
    return Redo(IGNORE_EXCEPTION_FOR_TESTING);
  }

  bool Undo(ExceptionState&) override {
    node_->setNodeValue(old_value_);
    return true;
  }

  bool Redo(ExceptionState&) override {
    node_->setNodeValue(value_);
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(node_);
    InspectorHistory::Action::Trace(visitor);
  }

 private:
  Member<Node> node_;
  String value_;
  String old_value_;
};

DOMEditor::DOMEditor(InspectorHistory* history) : history_(history) {}

bool DOMEditor::InsertBefore(ContainerNode* parent_node,
                             Node* node,
                             Node* anchor_node,
                             ExceptionState& exception_state) {
  return history_->Perform(
      MakeGarbageCollected<InsertBeforeAction>(parent_node, node, anchor_node),
      exception_state);
}

bool DOMEditor::RemoveChild(ContainerNode* parent_node,
                            Node* node,
                            ExceptionState& exception_state) {
  return history_->Perform(
      MakeGarbageCollected<RemoveChildAction>(parent_node, node),
      exception_state);
}

bool DOMEditor::SetAttribute(Element* element,
                             const String& name,
                             const String& value,
                             ExceptionState& exception_state) {
  return history_->Perform(
      MakeGarbageCollected<SetAttributeAction>(element, AtomicString(name),
                                               AtomicString(value)),
      exception_state);
}

bool DOMEditor::RemoveAttribute(Element* element,
                                const String& name,
                                ExceptionState& exception_state) {
  return history_->Perform(
      MakeGarbageCollected<RemoveAttributeAction>(element, AtomicString(name)),
      exception_state);
}

bool DOMEditor::SetOuterHTML(Node* node,
                             const String& html,
                             Node** new_node,
                             ExceptionState& exception_state) {
  SetOuterHTMLAction* action =
      MakeGarbageCollected<SetOuterHTMLAction>(node, html);
  bool result = history_->Perform(action, exception_state);
  if (result)
    *new_node = action->NewNode();
  return result;
}

bool DOMEditor::ReplaceChild(ContainerNode* parent_node,
                             Node* new_node,
                             Node* old_node,
                             ExceptionState& exception_state) {
  return history_->Perform(MakeGarbageCollected<ReplaceChildNodeAction>(
                               parent_node, new_node, old_node),
                           exception_state);
}

bool DOMEditor::SetNodeValue(Node* node,
                             const String& value,
                             ExceptionState& exception_state) {
  return history_->Perform(
      MakeGarbageCollected<SetNodeValueAction>(node, value), exception_state);
}

static protocol::Response ToResponse(ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    String name_prefix = IsDOMExceptionCode(exception_state.Code())
                             ? DOMException::GetErrorName(
                                   exception_state.CodeAs<DOMExceptionCode>()) +
                                   " "
                             : g_empty_string;
    String msg = name_prefix + exception_state.Message();
    return protocol::Response::ServerError(msg.Utf8());
  }
  return protocol::Response::Success();
}

protocol::Response DOMEditor::InsertBefore(ContainerNode* parent_node,
                                           Node* node,
                                           Node* anchor_node) {
  DummyExceptionStateForTesting exception_state;
  InsertBefore(parent_node, node, anchor_node, exception_state);
  return ToResponse(exception_state);
}

protocol::Response DOMEditor::RemoveChild(ContainerNode* parent_node,
                                          Node* node) {
  DummyExceptionStateForTesting exception_state;
  RemoveChild(parent_node, node, exception_state);
  return ToResponse(exception_state);
}

protocol::Response DOMEditor::SetAttribute(Element* element,
                                           const String& name,
                                           const String& value) {
  DummyExceptionStateForTesting exception_state;
  SetAttribute(element, name, value, exception_state);
  return ToResponse(exception_state);
}

protocol::Response DOMEditor::RemoveAttribute(Element* element,
                                              const String& name) {
  DummyExceptionStateForTesting exception_state;
  RemoveAttribute(element, name, exception_state);
  return ToResponse(exception_state);
}

protocol::Response DOMEditor::SetOuterHTML(Node* node,
                                           const String& html,
                                           Node** new_node) {
  DummyExceptionStateForTesting exception_state;
  SetOuterHTML(node, html, new_node, exception_state);
  return ToResponse(exception_state);
}

protocol::Response DOMEditor::SetNodeValue(Node* parent_node,
                                           const String& value) {
  DummyExceptionStateForTesting exception_state;
  SetNodeValue(parent_node, value, exception_state);
  return ToResponse(exception_state);
}

void DOMEditor::Trace(Visitor* visitor) const {
  visitor->Trace(history_);
}

}  // namespace blink
