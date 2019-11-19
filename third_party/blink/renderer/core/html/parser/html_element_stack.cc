/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_element_stack.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

namespace {

inline bool IsRootNode(HTMLStackItem* item) {
  return item->IsDocumentFragmentNode() ||
         item->HasTagName(html_names::kHTMLTag);
}

inline bool IsScopeMarker(HTMLStackItem* item) {
  return item->HasTagName(html_names::kAppletTag) ||
         item->HasTagName(html_names::kCaptionTag) ||
         item->HasTagName(html_names::kMarqueeTag) ||
         item->HasTagName(html_names::kObjectTag) ||
         item->HasTagName(html_names::kTableTag) ||
         item->HasTagName(html_names::kTdTag) ||
         item->HasTagName(html_names::kThTag) ||
         item->HasTagName(mathml_names::kMiTag) ||
         item->HasTagName(mathml_names::kMoTag) ||
         item->HasTagName(mathml_names::kMnTag) ||
         item->HasTagName(mathml_names::kMsTag) ||
         item->HasTagName(mathml_names::kMtextTag) ||
         item->HasTagName(mathml_names::kAnnotationXmlTag) ||
         item->HasTagName(svg_names::kForeignObjectTag) ||
         item->HasTagName(svg_names::kDescTag) ||
         item->HasTagName(svg_names::kTitleTag) ||
         item->HasTagName(html_names::kTemplateTag) || IsRootNode(item);
}

inline bool IsListItemScopeMarker(HTMLStackItem* item) {
  return IsScopeMarker(item) || item->HasTagName(html_names::kOlTag) ||
         item->HasTagName(html_names::kUlTag);
}

inline bool IsTableScopeMarker(HTMLStackItem* item) {
  return item->HasTagName(html_names::kTableTag) ||
         item->HasTagName(html_names::kTemplateTag) || IsRootNode(item);
}

inline bool IsTableBodyScopeMarker(HTMLStackItem* item) {
  return item->HasTagName(html_names::kTbodyTag) ||
         item->HasTagName(html_names::kTfootTag) ||
         item->HasTagName(html_names::kTheadTag) ||
         item->HasTagName(html_names::kTemplateTag) || IsRootNode(item);
}

inline bool IsTableRowScopeMarker(HTMLStackItem* item) {
  return item->HasTagName(html_names::kTrTag) ||
         item->HasTagName(html_names::kTemplateTag) || IsRootNode(item);
}

inline bool IsForeignContentScopeMarker(HTMLStackItem* item) {
  return HTMLElementStack::IsMathMLTextIntegrationPoint(item) ||
         HTMLElementStack::IsHTMLIntegrationPoint(item) ||
         item->IsInHTMLNamespace();
}

inline bool IsButtonScopeMarker(HTMLStackItem* item) {
  return IsScopeMarker(item) || item->HasTagName(html_names::kButtonTag);
}

inline bool IsSelectScopeMarker(HTMLStackItem* item) {
  return !item->HasTagName(html_names::kOptgroupTag) &&
         !item->HasTagName(html_names::kOptionTag);
}

}  // namespace

HTMLElementStack::ElementRecord::ElementRecord(HTMLStackItem* item,
                                               ElementRecord* next)
    : item_(item), next_(next) {
  DCHECK(item_);
}

void HTMLElementStack::ElementRecord::ReplaceElement(HTMLStackItem* item) {
  DCHECK(item);
  DCHECK(!item_ || item_->IsElementNode());
  // FIXME: Should this call finishParsingChildren?
  item_ = item;
}

bool HTMLElementStack::ElementRecord::IsAbove(ElementRecord* other) const {
  for (ElementRecord* below = Next(); below; below = below->Next()) {
    if (below == other)
      return true;
  }
  return false;
}

void HTMLElementStack::ElementRecord::Trace(Visitor* visitor) {
  visitor->Trace(item_);
  visitor->Trace(next_);
}

HTMLElementStack::HTMLElementStack()
    : root_node_(nullptr),
      head_element_(nullptr),
      body_element_(nullptr),
      stack_depth_(0) {}

HTMLElementStack::~HTMLElementStack() = default;

bool HTMLElementStack::HasOnlyOneElement() const {
  return !TopRecord()->Next();
}

bool HTMLElementStack::SecondElementIsHTMLBodyElement() const {
  // This is used the fragment case of <body> and <frameset> in the "in body"
  // insertion mode.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
  DCHECK(root_node_);
  // If we have a body element, it must always be the second element on the
  // stack, as we always start with an html element, and any other element
  // would cause the implicit creation of a body element.
  return !!body_element_;
}

void HTMLElementStack::PopHTMLHeadElement() {
  DCHECK_EQ(Top(), head_element_);
  head_element_ = nullptr;
  PopCommon();
}

void HTMLElementStack::PopHTMLBodyElement() {
  DCHECK_EQ(Top(), body_element_);
  body_element_ = nullptr;
  PopCommon();
}

void HTMLElementStack::PopAll() {
  root_node_ = nullptr;
  head_element_ = nullptr;
  body_element_ = nullptr;
  stack_depth_ = 0;
  while (top_) {
    Node& node = *TopNode();
    auto* element = DynamicTo<Element>(node);
    if (element) {
      element->FinishParsingChildren();
      if (auto* select = DynamicTo<HTMLSelectElement>(node))
        select->SetBlocksFormSubmission(true);
    }
    top_ = top_->ReleaseNext();
  }
}

void HTMLElementStack::Pop() {
  DCHECK(!TopStackItem()->HasTagName(html_names::kHeadTag));
  PopCommon();
}

void HTMLElementStack::PopUntil(const AtomicString& tag_name) {
  while (!TopStackItem()->MatchesHTMLTag(tag_name)) {
    // pop() will ASSERT if a <body>, <head> or <html> will be popped.
    Pop();
  }
}

void HTMLElementStack::PopUntilPopped(const AtomicString& tag_name) {
  PopUntil(tag_name);
  Pop();
}

void HTMLElementStack::PopUntilNumberedHeaderElementPopped() {
  while (!TopStackItem()->IsNumberedHeaderElement())
    Pop();
  Pop();
}

void HTMLElementStack::PopUntil(Element* element) {
  while (Top() != element)
    Pop();
}

void HTMLElementStack::PopUntilPopped(Element* element) {
  PopUntil(element);
  Pop();
}

void HTMLElementStack::PopUntilTableScopeMarker() {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-context
  while (!IsTableScopeMarker(TopStackItem()))
    Pop();
}

void HTMLElementStack::PopUntilTableBodyScopeMarker() {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-body-context
  while (!IsTableBodyScopeMarker(TopStackItem()))
    Pop();
}

void HTMLElementStack::PopUntilTableRowScopeMarker() {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-row-context
  while (!IsTableRowScopeMarker(TopStackItem()))
    Pop();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#mathml-text-integration-point
bool HTMLElementStack::IsMathMLTextIntegrationPoint(HTMLStackItem* item) {
  if (!item->IsElementNode())
    return false;
  return item->HasTagName(mathml_names::kMiTag) ||
         item->HasTagName(mathml_names::kMoTag) ||
         item->HasTagName(mathml_names::kMnTag) ||
         item->HasTagName(mathml_names::kMsTag) ||
         item->HasTagName(mathml_names::kMtextTag);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#html-integration-point
bool HTMLElementStack::IsHTMLIntegrationPoint(HTMLStackItem* item) {
  if (!item->IsElementNode())
    return false;
  if (item->HasTagName(mathml_names::kAnnotationXmlTag)) {
    Attribute* encoding_attr =
        item->GetAttributeItem(mathml_names::kEncodingAttr);
    if (encoding_attr) {
      const String& encoding = encoding_attr->Value();
      return DeprecatedEqualIgnoringCase(encoding, "text/html") ||
             DeprecatedEqualIgnoringCase(encoding, "application/xhtml+xml");
    }
    return false;
  }
  return item->HasTagName(svg_names::kForeignObjectTag) ||
         item->HasTagName(svg_names::kDescTag) ||
         item->HasTagName(svg_names::kTitleTag);
}

void HTMLElementStack::PopUntilForeignContentScopeMarker() {
  while (!IsForeignContentScopeMarker(TopStackItem()))
    Pop();
}

void HTMLElementStack::PushRootNode(HTMLStackItem* root_item) {
  DCHECK(root_item->IsDocumentFragmentNode());
  PushRootNodeCommon(root_item);
}

void HTMLElementStack::PushHTMLHtmlElement(HTMLStackItem* item) {
  DCHECK(item->HasTagName(html_names::kHTMLTag));
  PushRootNodeCommon(item);
}

void HTMLElementStack::PushRootNodeCommon(HTMLStackItem* root_item) {
  DCHECK(!top_);
  DCHECK(!root_node_);
  root_node_ = root_item->GetNode();
  PushCommon(root_item);
}

void HTMLElementStack::PushHTMLHeadElement(HTMLStackItem* item) {
  DCHECK(item->HasTagName(html_names::kHeadTag));
  DCHECK(!head_element_);
  head_element_ = item->GetElement();
  PushCommon(item);
}

void HTMLElementStack::PushHTMLBodyElement(HTMLStackItem* item) {
  DCHECK(item->HasTagName(html_names::kBodyTag));
  DCHECK(!body_element_);
  body_element_ = item->GetElement();
  PushCommon(item);
}

void HTMLElementStack::Push(HTMLStackItem* item) {
  DCHECK(!item->HasTagName(html_names::kHTMLTag));
  DCHECK(!item->HasTagName(html_names::kHeadTag));
  DCHECK(!item->HasTagName(html_names::kBodyTag));
  DCHECK(root_node_);
  PushCommon(item);
}

void HTMLElementStack::InsertAbove(HTMLStackItem* item,
                                   ElementRecord* record_below) {
  DCHECK(item);
  DCHECK(record_below);
  DCHECK(top_);
  DCHECK(!item->HasTagName(html_names::kHTMLTag));
  DCHECK(!item->HasTagName(html_names::kHeadTag));
  DCHECK(!item->HasTagName(html_names::kBodyTag));
  DCHECK(root_node_);
  if (record_below == top_) {
    Push(item);
    return;
  }

  for (ElementRecord* record_above = top_.Get(); record_above;
       record_above = record_above->Next()) {
    if (record_above->Next() != record_below)
      continue;

    stack_depth_++;
    record_above->SetNext(
        MakeGarbageCollected<ElementRecord>(item, record_above->ReleaseNext()));
    record_above->Next()->GetElement()->BeginParsingChildren();
    return;
  }
  NOTREACHED();
}

HTMLElementStack::ElementRecord* HTMLElementStack::TopRecord() const {
  DCHECK(top_);
  return top_.Get();
}

HTMLStackItem* HTMLElementStack::OneBelowTop() const {
  // We should never call this if there are fewer than 2 elements on the stack.
  DCHECK(top_);
  DCHECK(top_->Next());
  if (top_->Next()->StackItem()->IsElementNode())
    return top_->Next()->StackItem();
  return nullptr;
}

void HTMLElementStack::RemoveHTMLHeadElement(Element* element) {
  DCHECK_EQ(head_element_, element);
  if (top_->GetElement() == element) {
    PopHTMLHeadElement();
    return;
  }
  head_element_ = nullptr;
  RemoveNonTopCommon(element);
}

void HTMLElementStack::Remove(Element* element) {
  DCHECK(!IsA<HTMLHeadElement>(element));
  if (top_->GetElement() == element) {
    Pop();
    return;
  }
  RemoveNonTopCommon(element);
}

HTMLElementStack::ElementRecord* HTMLElementStack::Find(
    Element* element) const {
  for (ElementRecord* pos = top_.Get(); pos; pos = pos->Next()) {
    if (pos->GetNode() == element)
      return pos;
  }
  return nullptr;
}

HTMLElementStack::ElementRecord* HTMLElementStack::Topmost(
    const AtomicString& tag_name) const {
  for (ElementRecord* pos = top_.Get(); pos; pos = pos->Next()) {
    if (pos->StackItem()->MatchesHTMLTag(tag_name))
      return pos;
  }
  return nullptr;
}

bool HTMLElementStack::Contains(Element* element) const {
  return !!Find(element);
}

bool HTMLElementStack::Contains(const AtomicString& tag_name) const {
  return !!Topmost(tag_name);
}

template <bool isMarker(HTMLStackItem*)>
bool InScopeCommon(HTMLElementStack::ElementRecord* top,
                   const AtomicString& target_tag) {
  for (HTMLElementStack::ElementRecord* pos = top; pos; pos = pos->Next()) {
    HTMLStackItem* item = pos->StackItem();
    if (item->MatchesHTMLTag(target_tag))
      return true;
    if (isMarker(item))
      return false;
  }
  NOTREACHED();  // <html> is always on the stack and is a scope marker.
  return false;
}

bool HTMLElementStack::HasNumberedHeaderElementInScope() const {
  for (ElementRecord* record = top_.Get(); record; record = record->Next()) {
    HTMLStackItem* item = record->StackItem();
    if (item->IsNumberedHeaderElement())
      return true;
    if (IsScopeMarker(item))
      return false;
  }
  NOTREACHED();  // <html> is always on the stack and is a scope marker.
  return false;
}

bool HTMLElementStack::InScope(Element* target_element) const {
  for (ElementRecord* pos = top_.Get(); pos; pos = pos->Next()) {
    HTMLStackItem* item = pos->StackItem();
    if (item->GetNode() == target_element)
      return true;
    if (IsScopeMarker(item))
      return false;
  }
  NOTREACHED();  // <html> is always on the stack and is a scope marker.
  return false;
}

bool HTMLElementStack::InScope(const AtomicString& target_tag) const {
  return InScopeCommon<IsScopeMarker>(top_.Get(), target_tag);
}

bool HTMLElementStack::InScope(const QualifiedName& tag_name) const {
  return InScope(tag_name.LocalName());
}

bool HTMLElementStack::InListItemScope(const AtomicString& target_tag) const {
  return InScopeCommon<IsListItemScopeMarker>(top_.Get(), target_tag);
}

bool HTMLElementStack::InListItemScope(const QualifiedName& tag_name) const {
  return InListItemScope(tag_name.LocalName());
}

bool HTMLElementStack::InTableScope(const AtomicString& target_tag) const {
  return InScopeCommon<IsTableScopeMarker>(top_.Get(), target_tag);
}

bool HTMLElementStack::InTableScope(const QualifiedName& tag_name) const {
  return InTableScope(tag_name.LocalName());
}

bool HTMLElementStack::InButtonScope(const AtomicString& target_tag) const {
  return InScopeCommon<IsButtonScopeMarker>(top_.Get(), target_tag);
}

bool HTMLElementStack::InButtonScope(const QualifiedName& tag_name) const {
  return InButtonScope(tag_name.LocalName());
}

bool HTMLElementStack::InSelectScope(const AtomicString& target_tag) const {
  return InScopeCommon<IsSelectScopeMarker>(top_.Get(), target_tag);
}

bool HTMLElementStack::InSelectScope(const QualifiedName& tag_name) const {
  return InSelectScope(tag_name.LocalName());
}

bool HTMLElementStack::HasTemplateInHTMLScope() const {
  return InScopeCommon<IsRootNode>(top_.Get(),
                                   html_names::kTemplateTag.LocalName());
}

Element* HTMLElementStack::HtmlElement() const {
  DCHECK(root_node_);
  return To<Element>(root_node_.Get());
}

Element* HTMLElementStack::HeadElement() const {
  DCHECK(head_element_);
  return head_element_;
}

Element* HTMLElementStack::BodyElement() const {
  DCHECK(body_element_);
  return body_element_;
}

ContainerNode* HTMLElementStack::RootNode() const {
  DCHECK(root_node_);
  return root_node_;
}

void HTMLElementStack::PushCommon(HTMLStackItem* item) {
  DCHECK(root_node_);

  stack_depth_++;
  top_ = MakeGarbageCollected<ElementRecord>(item, top_.Release());
}

void HTMLElementStack::PopCommon() {
  DCHECK(!TopStackItem()->HasTagName(html_names::kHTMLTag));
  DCHECK(!TopStackItem()->HasTagName(html_names::kHeadTag) || !head_element_);
  DCHECK(!TopStackItem()->HasTagName(html_names::kBodyTag) || !body_element_);
  Top()->FinishParsingChildren();
  top_ = top_->ReleaseNext();

  stack_depth_--;
}

void HTMLElementStack::RemoveNonTopCommon(Element* element) {
  DCHECK(!IsA<HTMLHtmlElement>(element));
  DCHECK(!IsA<HTMLBodyElement>(element));
  DCHECK_NE(Top(), element);
  for (ElementRecord* pos = top_.Get(); pos; pos = pos->Next()) {
    if (pos->Next()->GetElement() == element) {
      // FIXME: Is it OK to call finishParsingChildren()
      // when the children aren't actually finished?
      element->FinishParsingChildren();
      pos->SetNext(pos->Next()->ReleaseNext());
      stack_depth_--;
      return;
    }
  }
  NOTREACHED();
}

HTMLElementStack::ElementRecord*
HTMLElementStack::FurthestBlockForFormattingElement(
    Element* formatting_element) const {
  ElementRecord* furthest_block = nullptr;
  for (ElementRecord* pos = top_.Get(); pos; pos = pos->Next()) {
    if (pos->GetElement() == formatting_element)
      return furthest_block;
    if (pos->StackItem()->IsSpecialNode())
      furthest_block = pos;
  }
  NOTREACHED();
  return nullptr;
}

void HTMLElementStack::Trace(Visitor* visitor) {
  visitor->Trace(top_);
  visitor->Trace(root_node_);
  visitor->Trace(head_element_);
  visitor->Trace(body_element_);
}

#ifndef NDEBUG

void HTMLElementStack::Show() {
  for (ElementRecord* record = top_.Get(); record; record = record->Next())
    LOG(INFO) << *record->GetElement();
}

#endif

}  // namespace blink
