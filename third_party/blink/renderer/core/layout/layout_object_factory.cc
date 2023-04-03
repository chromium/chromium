// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_ng_custom.h"
#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_fragment.h"
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
inline BaseType* CreateObject(Node& node,
                              LegacyLayout legacy,
                              bool disable_ng_for_type = false) {
  Element* element = GetElementForLayoutObject(node);
  bool force_legacy = false;

  // If no reason has been found for disabling NG for this particular type,
  // check if the NG feature is enabled at all, before considering creating an
  // NG object.
  if (!disable_ng_for_type) {
    // The last thing to check is whether we should force legacy layout. This
    // happens when the NG feature is enabled for the object in question, but
    // we're dealing with something that isn't implemented in NG yet (such as
    // editing or multicol). We then need to force legacy layout for the entire
    // subtree.
    force_legacy = legacy == LegacyLayout::kForce;

    if (!force_legacy)
      return MakeGarbageCollected<NGType>(element);
  }
  BaseType* new_object = MakeGarbageCollected<LegacyType>(element);
  if (force_legacy)
    new_object->SetForceLegacyLayout();
  return new_object;
}

}  // anonymous namespace

LayoutBlockFlow* LayoutObjectFactory::CreateBlockFlow(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (style.Display() == EDisplay::kListItem &&
      node.GetPseudoId() != kPseudoIdBackdrop) {
    // Create a LayoutBlockFlow with a ListItemOrdinal and maybe a ::marker.
    // ::backdrop is excluded since it's not tree-abiding, and ListItemOrdinal
    // needs to traverse the tree.
    return CreateObject<LayoutBlockFlow, LayoutNGListItem, LayoutListItem>(
        node, legacy);
  }

  // Create a plain LayoutBlockFlow
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow>(node, legacy);
}

LayoutObject* LayoutObjectFactory::CreateListMarker(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  const Node* parent = node.parentNode();
  const ComputedStyle* parent_style = parent->GetComputedStyle();

  if (legacy == LegacyLayout::kForce) {
    NOTREACHED();
  }
  bool is_inside =
      parent_style->ListStylePosition() == EListStylePosition::kInside ||
      (IsA<HTMLLIElement>(parent) && !parent_style->IsInsideListElement());
  if (style.ContentBehavesAsNormal()) {
    if (is_inside) {
      return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                          LayoutListMarker>(node, legacy);
    }
    return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                        LayoutListMarker>(node, legacy);
  }
  if (is_inside) {
    return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                        LayoutInsideListMarker>(node, legacy);
  }
  return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                      LayoutOutsideListMarker>(node, legacy);
}

LayoutText* LayoutObjectFactory::CreateText(Node* node,
                                            String str,
                                            LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGText>(node, std::move(str));
  }
  LayoutText* layout_text =
      MakeGarbageCollected<LayoutText>(node, std::move(str));
  if (force_legacy)
    layout_text->SetForceLegacyLayout();
  return layout_text;
}

LayoutTextFragment* LayoutObjectFactory::CreateTextFragment(
    Node* node,
    const String& str,
    int start_offset,
    int length,
    LegacyLayout legacy) {
  bool force_legacy = legacy == LegacyLayout::kForce;
  if (!force_legacy) {
    return MakeGarbageCollected<LayoutNGTextFragment>(node, str, start_offset,
                                                      length);
  }
  LayoutTextFragment* layout_text_fragment =
      MakeGarbageCollected<LayoutTextFragment>(node, str, start_offset, length);
  if (force_legacy)
    layout_text_fragment->SetForceLegacyLayout();
  return layout_text_fragment;
}

}  // namespace blink
