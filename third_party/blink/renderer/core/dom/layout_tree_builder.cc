/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/generated_children.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

LayoutTreeBuilderForElement::LayoutTreeBuilderForElement(
    Element& element,
    Node::AttachContext& context,
    const ComputedStyle* style)
    : LayoutTreeBuilder(element, context, style) {
  DCHECK(style_);
  DCHECK(!style_->IsEnsuredInDisplayNone());
}

LayoutObject* LayoutTreeBuilderForElement::NextLayoutObject() const {
  if (node_->IsFirstLetterPseudoElement()) {
    return context_.next_sibling;
  }
  // ::scroll-marker pseudo elements are always attached one after another.
  if (node_->IsScrollMarkerPseudoElement()) {
    return nullptr;
  }
  if (style_->IsRenderedInTopLayer(*node_)) {
    if (LayoutObject* next_in_top_layer =
            LayoutTreeBuilderTraversal::NextInTopLayer(*node_)) {
      return next_in_top_layer;
    }

    // We are at the end of the top layer elements. If we're in a transition,
    // the ::view-transition is rendered on top of the top layer elements and
    // its "snapshot containing block" is appended as the last child of the
    // LayoutView. Otherwise, this returns nullptr and we're at the end.
    return node_->GetDocument().GetLayoutView()->GetViewTransitionRoot();
  }
  return LayoutTreeBuilder::NextLayoutObject();
}

LayoutObject* LayoutTreeBuilderForElement::ParentLayoutObject() const {
  if (style_->IsRenderedInTopLayer(*node_)) {
    return node_->GetDocument().GetLayoutView();
  }
#if DCHECK_IS_ON()
  // Box of ::scroll-marker-group is previous/next sibling of
  // its originating element, so the parent should be originating element's
  // parent.
  if (node_->IsScrollMarkerGroupPseudoElement()) {
    Element* originating_element =
        To<PseudoElement>(node_)->OriginatingElement();
    ContainerNode* parent_element =
        LayoutTreeBuilderTraversal::LayoutParent(*originating_element);
    DCHECK_EQ(parent_element->GetLayoutObject(), context_.parent);
  }
#endif  // DCHECK_IS_ON()
  return context_.parent;
}

DISABLE_CFI_PERF
void LayoutTreeBuilderForElement::CreateLayoutObject() {
  LayoutObject* parent_layout_object = ParentLayoutObject();
  if (!parent_layout_object)
    return;
  if (!parent_layout_object->CanHaveChildren())
    return;

  // If we are in the top layer and the parent layout object without top layer
  // adjustment can't have children, then don't render.
  // https://github.com/w3c/csswg-drafts/issues/6939#issuecomment-1016671534
  if (style_->IsRenderedInTopLayer(*node_) && context_.parent &&
      !context_.parent->CanHaveChildren() &&
      node_->GetPseudoId() != kPseudoIdBackdrop) {
    return;
  }

  if (node_->IsPseudoElement() &&
      !CanHaveGeneratedChildren(*parent_layout_object))
    return;
  if (!node_->LayoutObjectIsNeeded(*style_))
    return;

  LayoutObject* new_layout_object = node_->CreateLayoutObject(*style_);
  if (!new_layout_object)
    return;

  if (!parent_layout_object->IsChildAllowed(new_layout_object, *style_)) {
    new_layout_object->Destroy();
    return;
  }

  // Make sure the LayoutObject already knows it is going to be added to a
  // LayoutFlowThread before we set the style for the first time. Otherwise code
  // using IsInsideFlowThread() in the StyleWillChange and StyleDidChange will
  // fail.
  new_layout_object->SetIsInsideFlowThread(
      parent_layout_object->IsInsideFlowThread());

  LayoutObject* next_layout_object = NextLayoutObject();
  node_->SetLayoutObject(new_layout_object);

  DCHECK(!new_layout_object->Style());
  new_layout_object->SetStyle(style_);

  parent_layout_object->AddChild(new_layout_object, next_layout_object);
}

const ComputedStyle*
LayoutTreeBuilderForText::CreateInlineWrapperStyleForDisplayContentsIfNeeded()
    const {
  // If the parent element is not a display:contents element, the style and the
  // parent style will be the same ComputedStyle object. Early out here.
  if (style_ == context_.parent->Style())
    return nullptr;

  return node_->GetDocument()
      .GetStyleResolver()
      .CreateInheritedDisplayContentsStyleIfNeeded(*style_,
                                                   context_.parent->StyleRef());
}

LayoutObject*
LayoutTreeBuilderForText::CreateInlineWrapperForDisplayContentsIfNeeded(
    const ComputedStyle* wrapper_style) const {
  if (!wrapper_style)
    return nullptr;

  // Text nodes which are children of display:contents element which modifies
  // inherited properties, need to have an anonymous inline wrapper having those
  // inherited properties because the layout code expects the LayoutObject
  // parent of text nodes to have the same inherited properties.
  LayoutObject* inline_wrapper =
      LayoutInline::CreateAnonymous(&node_->GetDocument());
  inline_wrapper->SetStyle(wrapper_style);
  if (!context_.parent->IsChildAllowed(inline_wrapper, *wrapper_style)) {
    inline_wrapper->Destroy();
    return nullptr;
  }
  context_.parent->AddChild(inline_wrapper, NextLayoutObject());
  return inline_wrapper;
}

void LayoutTreeBuilderForText::CreateLayoutObject() {
  const ComputedStyle* style = style_;
  LayoutObject* layout_object_parent = context_.parent;
  LayoutObject* next_layout_object = NextLayoutObject();
  const ComputedStyle* nullable_wrapper_style =
      CreateInlineWrapperStyleForDisplayContentsIfNeeded();
  if (LayoutObject* wrapper = CreateInlineWrapperForDisplayContentsIfNeeded(
          nullable_wrapper_style)) {
    layout_object_parent = wrapper;
    next_layout_object = nullptr;
  }
  // SVG <text> doesn't accept anonymous LayoutInlines. But the Text should have
  // the adjusted ComputedStyle.
  if (nullable_wrapper_style)
    style = nullable_wrapper_style;

  LayoutText* new_layout_object = node_->CreateTextLayoutObject();
  if (!layout_object_parent->IsChildAllowed(new_layout_object, *style)) {
    new_layout_object->Destroy();
    return;
  }

  // Make sure the LayoutObject already knows it is going to be added to a
  // LayoutFlowThread before we set the style for the first time. Otherwise code
  // using IsInsideFlowThread() in the StyleWillChange and StyleDidChange will
  // fail.
  new_layout_object->SetIsInsideFlowThread(
      context_.parent->IsInsideFlowThread());

  node_->SetLayoutObject(new_layout_object);
  DCHECK(!new_layout_object->Style());
  new_layout_object->SetStyle(style);

  layout_object_parent->AddChild(new_layout_object, next_layout_object);
}

}  // namespace blink
