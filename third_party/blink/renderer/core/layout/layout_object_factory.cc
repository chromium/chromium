// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline bool ShouldUseNewLayout(Document& document, const ComputedStyle& style) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return false;
  bool requires_ng_block_fragmentation =
      document.Printing() ||
      (document.GetLayoutView() &&
       document.GetLayoutView()->StyleRef().IsOverflowPaged());
  if (requires_ng_block_fragmentation &&
      !RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled())
    return false;
  return !style.ForceLegacyLayout();
}

inline Element* GetElementForLayoutObject(Node& node) {
  if (node.IsElementNode())
    return &ToElement(node);
  // If |node| is a Document, the layout object is going to be anonymous.
  DCHECK(node.IsDocumentNode());
  return nullptr;
}

}  // anonymous namespace

LayoutBlockFlow* LayoutObjectFactory::CreateBlockFlow(
    Node& node,
    const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (ShouldUseNewLayout(node.GetDocument(), style))
    return new LayoutNGBlockFlow(element);
  return new LayoutBlockFlow(element);
}

LayoutBlock* LayoutObjectFactory::CreateFlexibleBox(
    Node& node,
    const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (RuntimeEnabledFeatures::LayoutNGFlexBoxEnabled() &&
      ShouldUseNewLayout(node.GetDocument(), style))
    return new LayoutNGFlexibleBox(element);
  return new LayoutFlexibleBox(element);
}

LayoutBlockFlow* LayoutObjectFactory::CreateListItem(
    Node& node,
    const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (ShouldUseNewLayout(node.GetDocument(), style))
    return new LayoutNGListItem(element);
  return new LayoutListItem(element);
}

LayoutTableCaption* LayoutObjectFactory::CreateTableCaption(
    Node& node,
    const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (ShouldUseNewLayout(node.GetDocument(), style))
    return new LayoutNGTableCaption(element);
  return new LayoutTableCaption(element);
}

LayoutTableCell* LayoutObjectFactory::CreateTableCell(
    Node& node,
    const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (ShouldUseNewLayout(node.GetDocument(), style))
    return new LayoutNGTableCell(element);
  return new LayoutTableCell(element);
}

LayoutBlock* LayoutObjectFactory::CreateFieldset(Node& node,
                                                 const ComputedStyle& style) {
  Element* element = GetElementForLayoutObject(node);
  if (RuntimeEnabledFeatures::LayoutNGFieldsetEnabled() &&
      ShouldUseNewLayout(node.GetDocument(), style)) {
    return new LayoutNGFieldset(element);
  }
  return new LayoutFieldset(element);
}

}  // namespace blink
