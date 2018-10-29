/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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
 */

#include "third_party/blink/renderer/core/dom/text.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Text* Text::Create(Document& document, const String& data) {
  return new Text(document, data, kCreateText);
}

Text* Text::CreateEditingText(Document& document, const String& data) {
  return new Text(document, data, kCreateEditingText);
}

Node* Text::MergeNextSiblingNodesIfPossible() {
  // Remove empty text nodes.
  if (!length()) {
    // Care must be taken to get the next node before removing the current node.
    Node* next_node = NodeTraversal::NextPostOrder(*this);
    remove(IGNORE_EXCEPTION_FOR_TESTING);
    return next_node;
  }

  // Merge text nodes.
  while (Node* next_sibling = nextSibling()) {
    if (next_sibling->getNodeType() != kTextNode)
      break;

    Text* next_text = ToText(next_sibling);

    // Remove empty text nodes.
    if (!next_text->length()) {
      next_text->remove(IGNORE_EXCEPTION_FOR_TESTING);
      continue;
    }

    // Both non-empty text nodes. Merge them.
    unsigned offset = length();
    String next_text_data = next_text->data();
    String old_text_data = data();
    SetDataWithoutUpdate(data() + next_text_data);
    UpdateTextLayoutObject(old_text_data.length(), 0);

    GetDocument().DidMergeTextNodes(*this, *next_text, offset);

    // Empty nextText for layout update.
    next_text->SetDataWithoutUpdate(g_empty_string);
    next_text->UpdateTextLayoutObject(0, next_text_data.length());

    // Restore nextText for mutation event.
    next_text->SetDataWithoutUpdate(next_text_data);
    next_text->UpdateTextLayoutObject(0, 0);

    GetDocument().IncDOMTreeVersion();
    DidModifyData(old_text_data, CharacterData::kUpdateFromNonParser);
    next_text->remove(IGNORE_EXCEPTION_FOR_TESTING);
  }

  return NodeTraversal::NextPostOrder(*this);
}

Text* Text::splitText(unsigned offset, ExceptionState& exception_state) {
  // IndexSizeError: Raised if the specified offset is negative or greater than
  // the number of 16-bit units in data.
  if (offset > length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The offset " + String::Number(offset) +
            " is larger than the Text node's length.");
    return nullptr;
  }

  EventQueueScope scope;
  String old_str = data();
  Text* new_text = CloneWithData(GetDocument(), old_str.Substring(offset));
  SetDataWithoutUpdate(old_str.Substring(0, offset));

  DidModifyData(old_str, CharacterData::kUpdateFromNonParser);

  if (parentNode())
    parentNode()->InsertBefore(new_text, nextSibling(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (GetLayoutObject())
    GetLayoutObject()->SetTextWithOffset(DataImpl(), 0, old_str.length());

  if (parentNode())
    GetDocument().DidSplitTextNode(*this);
  else
    GetDocument().DidRemoveText(*this, offset, old_str.length() - offset);

  // [NewObject] must always create a new wrapper.  Check that a wrapper
  // does not exist yet.
  DCHECK(
      DOMDataStore::GetWrapper(new_text, v8::Isolate::GetCurrent()).IsEmpty());

  return new_text;
}

static const Text* EarliestLogicallyAdjacentTextNode(const Text* t) {
  for (const Node* n = t->previousSibling(); n; n = n->previousSibling()) {
    Node::NodeType type = n->getNodeType();
    if (type == Node::kTextNode || type == Node::kCdataSectionNode) {
      t = ToText(n);
      continue;
    }

    break;
  }
  return t;
}

static const Text* LatestLogicallyAdjacentTextNode(const Text* t) {
  for (const Node* n = t->nextSibling(); n; n = n->nextSibling()) {
    Node::NodeType type = n->getNodeType();
    if (type == Node::kTextNode || type == Node::kCdataSectionNode) {
      t = ToText(n);
      continue;
    }

    break;
  }
  return t;
}

String Text::wholeText() const {
  const Text* start_text = EarliestLogicallyAdjacentTextNode(this);
  const Text* end_text = LatestLogicallyAdjacentTextNode(this);

  Node* one_past_end_text = end_text->nextSibling();
  unsigned result_length = 0;
  for (const Node* n = start_text; n != one_past_end_text;
       n = n->nextSibling()) {
    if (!n->IsTextNode())
      continue;
    const String& data = ToText(n)->data();
    CHECK_GE(std::numeric_limits<unsigned>::max() - data.length(),
             result_length);
    result_length += data.length();
  }
  StringBuilder result;
  result.ReserveCapacity(result_length);
  for (const Node* n = start_text; n != one_past_end_text;
       n = n->nextSibling()) {
    if (!n->IsTextNode())
      continue;
    result.Append(ToText(n)->data());
  }
  DCHECK_EQ(result.length(), result_length);

  return result.ToString();
}

Text* Text::ReplaceWholeText(const String& new_text) {
  // Remove all adjacent text nodes, and replace the contents of this one.

  // Protect startText and endText against mutation event handlers removing the
  // last ref
  Text* start_text = const_cast<Text*>(EarliestLogicallyAdjacentTextNode(this));
  Text* end_text = const_cast<Text*>(LatestLogicallyAdjacentTextNode(this));

  ContainerNode* parent = parentNode();  // Protect against mutation handlers
                                         // moving this node during traversal
  for (Node* n = start_text;
       n && n != this && n->IsTextNode() && n->parentNode() == parent;) {
    Node* node_to_remove = n;
    n = node_to_remove->nextSibling();
    parent->RemoveChild(node_to_remove, IGNORE_EXCEPTION_FOR_TESTING);
  }

  if (this != end_text) {
    Node* one_past_end_text = end_text->nextSibling();
    for (Node* n = nextSibling(); n && n != one_past_end_text &&
                                  n->IsTextNode() &&
                                  n->parentNode() == parent;) {
      Node* node_to_remove = n;
      n = node_to_remove->nextSibling();
      parent->RemoveChild(node_to_remove, IGNORE_EXCEPTION_FOR_TESTING);
    }
  }

  if (new_text.IsEmpty()) {
    if (parent && parentNode() == parent)
      parent->RemoveChild(this, IGNORE_EXCEPTION_FOR_TESTING);
    return nullptr;
  }

  setData(new_text);
  return this;
}

String Text::nodeName() const {
  return "#text";
}

Node::NodeType Text::getNodeType() const {
  return kTextNode;
}

Node* Text::Clone(Document& factory, CloneChildrenFlag) const {
  return CloneWithData(factory, data());
}

static inline bool EndsWithWhitespace(const String& text) {
  return text.length() && IsASCIISpace(text[text.length() - 1]);
}

static inline bool CanHaveWhitespaceChildren(
    const LayoutObject& parent,
    const ComputedStyle& style,
    const Text::AttachContext& context) {
  // <button> and <fieldset> should allow whitespace even though
  // LayoutFlexibleBox doesn't.
  if (parent.IsLayoutButton() || parent.IsFieldset())
    return true;

  if (parent.IsTable() || parent.IsTableRow() || parent.IsTableSection() ||
      parent.IsLayoutTableCol() || parent.IsFrameSet() ||
      parent.IsFlexibleBox() || parent.IsLayoutGrid() || parent.IsSVGRoot() ||
      parent.IsSVGContainer() || parent.IsSVGImage() || parent.IsSVGShape()) {
    if (!context.use_previous_in_flow || !context.previous_in_flow ||
        !context.previous_in_flow->IsText())
      return false;

    return style.PreserveNewline() ||
           !EndsWithWhitespace(
               ToLayoutText(context.previous_in_flow)->GetText());
  }
  return true;
}

bool Text::TextLayoutObjectIsNeeded(const AttachContext& context,
                                    const ComputedStyle& style,
                                    const LayoutObject& parent) const {
  DCHECK(!GetDocument().ChildNeedsDistributionRecalc());

  if (!parent.CanHaveChildren())
    return false;

  if (IsEditingText())
    return true;

  if (!length())
    return false;

  if (style.Display() == EDisplay::kNone)
    return false;

  if (!ContainsOnlyWhitespaceOrEmpty())
    return true;

  if (!CanHaveWhitespaceChildren(parent, style, context))
    return false;

  // pre-wrap in SVG never makes layoutObject.
  if (style.WhiteSpace() == EWhiteSpace::kPreWrap && parent.IsSVG())
    return false;

  // pre/pre-wrap/pre-line always make layoutObjects.
  if (style.PreserveNewline())
    return true;

  if (!context.use_previous_in_flow)
    return false;

  if (!context.previous_in_flow)
    return parent.IsLayoutInline();

  if (context.previous_in_flow->IsText()) {
    return !EndsWithWhitespace(
        ToLayoutText(context.previous_in_flow)->GetText());
  }

  return context.previous_in_flow->IsInline() &&
         !context.previous_in_flow->IsBR();
}

static bool IsSVGText(Text* text) {
  Node* parent_or_shadow_host_node = text->ParentOrShadowHostNode();
  DCHECK(parent_or_shadow_host_node);
  return parent_or_shadow_host_node->IsSVGElement() &&
         !IsSVGForeignObjectElement(*parent_or_shadow_host_node);
}

LayoutText* Text::CreateTextLayoutObject(const ComputedStyle& style) {
  if (IsSVGText(this))
    return new LayoutSVGInlineText(this, DataImpl());

  if (style.HasTextCombine())
    return new LayoutTextCombine(this, DataImpl());

  if (RuntimeEnabledFeatures::LayoutNGEnabled() && !style.ForceLegacyLayout())
    return new LayoutNGText(this, DataImpl());

  return new LayoutText(this, DataImpl());
}

void Text::AttachLayoutTree(AttachContext& context) {
  ContainerNode* style_parent = LayoutTreeBuilderTraversal::Parent(*this);
  LayoutObject* parent_layout_object =
      LayoutTreeBuilderTraversal::ParentLayoutObject(*this);

  if (style_parent && parent_layout_object) {
    DCHECK(style_parent->GetComputedStyle());
    if (TextLayoutObjectIsNeeded(context, *style_parent->GetComputedStyle(),
                                 *parent_layout_object)) {
      LayoutTreeBuilderForText(*this, parent_layout_object,
                               style_parent->MutableComputedStyle())
          .CreateLayoutObject();
      context.previous_in_flow = GetLayoutObject();
    }
  }
  CharacterData::AttachLayoutTree(context);
}

void Text::ReattachLayoutTreeIfNeeded(const AttachContext& context) {
  bool layout_object_is_needed = false;
  ContainerNode* style_parent = LayoutTreeBuilderTraversal::Parent(*this);
  LayoutObject* parent_layout_object =
      LayoutTreeBuilderTraversal::ParentLayoutObject(*this);
  if (style_parent && parent_layout_object) {
    DCHECK(style_parent->GetComputedStyle());
    layout_object_is_needed = TextLayoutObjectIsNeeded(
        context, *style_parent->GetComputedStyle(), *parent_layout_object);
  }

  if (layout_object_is_needed == !!GetLayoutObject())
    return;

  // The following is almost the same as Node::reattachLayoutTree() except that
  // we create a layoutObject only if needed.  Not calling reattachLayoutTree()
  // to avoid repeated calls to Text::textLayoutObjectIsNeeded().
  AttachContext reattach_context;
  reattach_context.performing_reattach = true;

  if (GetStyleChangeType() < kNeedsReattachStyleChange)
    DetachLayoutTree(reattach_context);
  if (layout_object_is_needed) {
    LayoutTreeBuilderForText(*this, parent_layout_object,
                             style_parent->MutableComputedStyle())
        .CreateLayoutObject();
  }
  CharacterData::AttachLayoutTree(reattach_context);
}

void Text::RecalcTextStyle(StyleRecalcChange change) {
  if (LayoutText* layout_text = GetLayoutObject()) {
    if (change != kNoChange || NeedsStyleRecalc()) {
      scoped_refptr<ComputedStyle> new_style =
          GetDocument().EnsureStyleResolver().StyleForText(this);
      const ComputedStyle* layout_parent_style =
          GetLayoutObject()->Parent()->Style();
      if (new_style != layout_parent_style &&
          !new_style->InheritedEqual(*layout_parent_style)) {
        // The computed style or the need for an anonymous inline wrapper for a
        // display:contents text child changed.
        SetNeedsReattachLayoutTree();
        return;
      }
      layout_text->SetStyle(std::move(new_style));
    }
    if (NeedsStyleRecalc())
      layout_text->SetText(DataImpl());
    ClearNeedsStyleRecalc();
  } else if (NeedsStyleRecalc() || NeedsWhitespaceLayoutObject()) {
    SetNeedsReattachLayoutTree();
  }
}

void Text::RebuildTextLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  DCHECK(!ChildNeedsStyleRecalc());
  DCHECK(NeedsReattachLayoutTree());
  DCHECK(parentNode());

  ReattachLayoutTree();
  whitespace_attacher.DidReattachText(this);
  ClearNeedsReattachLayoutTree();
}

// If a whitespace node had no layoutObject and goes through a recalcStyle it
// may need to create one if the parent style now has white-space: pre.
bool Text::NeedsWhitespaceLayoutObject() {
  DCHECK(!GetLayoutObject());
  if (const ComputedStyle* style = ParentComputedStyle())
    return style->PreserveNewline();
  return false;
}

// Passing both |textNode| and its layout object because repeated calls to
// |Node::layoutObject()| are discouraged.
static bool ShouldUpdateLayoutByReattaching(const Text& text_node,
                                            LayoutText* text_layout_object) {
  DCHECK_EQ(text_node.GetLayoutObject(), text_layout_object);
  if (!text_layout_object)
    return true;
  // In general we do not want to branch on lifecycle states such as
  // |childNeedsDistributionRecalc|, but this code tries to figure out if we can
  // use an optimized code path that avoids reattach.
  if (!text_node.GetDocument().ChildNeedsDistributionRecalc() &&
      !text_node.TextLayoutObjectIsNeeded(Node::AttachContext(),
                                          *text_layout_object->Style(),
                                          *text_layout_object->Parent())) {
    return true;
  }
  if (text_layout_object->IsTextFragment()) {
    // Changes of |textNode| may change first letter part, so we should
    // reattach.
    return ToLayoutTextFragment(text_layout_object)
        ->GetFirstLetterPseudoElement();
  }
  return false;
}

void Text::UpdateTextLayoutObject(unsigned offset_of_replaced_data,
                                  unsigned length_of_replaced_data) {
  if (!InActiveDocument())
    return;
  LayoutText* text_layout_object = GetLayoutObject();
  if (ShouldUpdateLayoutByReattaching(*this, text_layout_object)) {
    LazyReattachIfAttached();
    return;
  }

  text_layout_object->SetTextWithOffset(DataImpl(), offset_of_replaced_data,
                                        length_of_replaced_data);
}

Text* Text::CloneWithData(Document& factory, const String& data) const {
  return Create(factory, data);
}

void Text::Trace(blink::Visitor* visitor) {
  CharacterData::Trace(visitor);
}

}  // namespace blink
