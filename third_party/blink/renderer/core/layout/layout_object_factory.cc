// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_as_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block_flow.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline Element* GetElementForLayoutObject(Node& node) {
  if (auto* element = DynamicTo<Element>(node))
    return element;
  // If |node| is a Document, the layout object is going to be anonymous.
  DCHECK(node.IsDocumentNode());
  return nullptr;
}

template <typename BaseType, typename NGType, typename LegacyType = BaseType>
inline BaseType* CreateObject(Node& node, bool disable_ng_for_type = false) {
  Element* element = GetElementForLayoutObject(node);
  return MakeGarbageCollected<NGType>(element);
}

}  // anonymous namespace

LayoutBlockFlow* LayoutObjectFactory::CreateBlockFlow(
    Node& node,
    const ComputedStyle& style) {
  if (style.Display() == EDisplay::kListItem &&
      node.GetPseudoId() != kPseudoIdBackdrop) {
    // Create a LayoutBlockFlow with a ListItemOrdinal and maybe a ::marker.
    // ::backdrop is excluded since it's not tree-abiding, and ListItemOrdinal
    // needs to traverse the tree.
    return CreateObject<LayoutBlockFlow, LayoutNGListItem, LayoutListItem>(
        node);
  }

  // Create a plain LayoutBlockFlow
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow>(node);
}

LayoutObject* LayoutObjectFactory::CreateListMarker(
    Node& node,
    const ComputedStyle& style) {
  const Node* parent = node.parentNode();
  const ComputedStyle* parent_style = parent->GetComputedStyle();

  bool is_inside =
      parent_style->ListStylePosition() == EListStylePosition::kInside ||
      (IsA<HTMLLIElement>(parent) && !parent_style->IsInsideListElement());
  if (style.ContentBehavesAsNormal()) {
    if (is_inside) {
      return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                          LayoutListMarker>(node);
    }
    return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                        LayoutListMarker>(node);
  }
  if (is_inside) {
    return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                        LayoutInsideListMarker>(node);
  }
  return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                      LayoutOutsideListMarker>(node);
}

}  // namespace blink
