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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_LAYOUT_TREE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_LAYOUT_TREE_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

class ComputedStyle;

// The LayoutTreeBuilder class uses the DOM tree and CSS style rules as input to
// form a LayoutObject Tree which is then used for layout computations in a
// later stage.

// To construct the LayoutObject tree, the LayoutTreeBuilder does the following:
// 1. Starting at the root of the DOM tree, traverse each visible node.
//    Visibility is determined by
//    LayoutTreeBuilderFor{Element,Text}::ShouldCreateLayoutObject() functions.
// 2. For each visible node, ensure that the style has been resolved (either by
//    getting the ComputedStyle passed on to the LayoutTreeBuilder or by forcing
//    style resolution). This is done in LayoutTreeBuilderForElement::Style().
// 3. Emit visible LayoutObjects with content and their computed styles.
//    This is dealt with by the
//    LayoutTreeBuilderFor{Element,Text}::CreateLayoutObject() functions.
template <typename NodeType>
class LayoutTreeBuilder {
  STACK_ALLOCATED();

 protected:
  LayoutTreeBuilder(NodeType& node,
                    Node::AttachContext& context,
                    const ComputedStyle* style)
      : node_(node), context_(context), style_(style) {
    DCHECK(!node.GetLayoutObject());
    DCHECK(node.GetDocument().InStyleRecalc());
    DCHECK(node.InActiveDocument());
    DCHECK(context.parent);
  }

  LayoutObject* NextLayoutObject() const {
    if (!context_.next_sibling_valid) {
      context_.next_sibling =
          LayoutTreeBuilderTraversal::NextSiblingLayoutObject(*node_);
      context_.next_sibling_valid = true;
    }
    LayoutObject* next = context_.next_sibling;
    // If a text node is wrapped in an anonymous inline for display:contents
    // (see CreateInlineWrapperForDisplayContents()), use the wrapper as the
    // next layout object. Otherwise we would need to add code to various
    // AddChild() implementations to walk up the tree to find the correct
    // layout tree parent/siblings.
    if (next && next->IsText() && next->Parent()->IsAnonymous() &&
        next->Parent()->IsInline()) {
      return next->Parent();
    }
    return next;
  }

  Member<NodeType> node_;
  Node::AttachContext& context_;
  const ComputedStyle* style_;
};

class LayoutTreeBuilderForElement : public LayoutTreeBuilder<Element> {
 public:
  LayoutTreeBuilderForElement(Element&,
                              Node::AttachContext&,
                              const ComputedStyle*,
                              LegacyLayout legacy);

  void CreateLayoutObject();

 private:
  LayoutObject* ParentLayoutObject() const;
  LayoutObject* NextLayoutObject() const;

  LegacyLayout legacy_;
};

class LayoutTreeBuilderForText : public LayoutTreeBuilder<Text> {
 public:
  LayoutTreeBuilderForText(Text& text,
                           Node::AttachContext& context,
                           const ComputedStyle* style_from_parent)
      : LayoutTreeBuilder(text, context, style_from_parent) {}

  void CreateLayoutObject();

 private:
  LayoutObject* CreateInlineWrapperForDisplayContentsIfNeeded();
};

}  // namespace blink

#endif
