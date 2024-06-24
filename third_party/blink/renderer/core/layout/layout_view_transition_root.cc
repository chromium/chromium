// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_view_transition_root.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_style_tracker.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

namespace blink {

LayoutViewTransitionRoot::LayoutViewTransitionRoot(Document& document)
    : LayoutBlockFlow(nullptr) {
  SetDocumentForAnonymous(&document);
  SetChildrenInline(false);

  // Create an empty initial style so we can be added to the tree before
  // UpdateSnapshotStyle is called.
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          GetDocument().GetLayoutView()->StyleRef(), EDisplay::kBlock);
  SetStyle(new_style_builder.TakeStyle());
}

LayoutViewTransitionRoot::~LayoutViewTransitionRoot() = default;

void LayoutViewTransitionRoot::UpdateSnapshotStyle(
    const ViewTransitionStyleTracker& style_tracker) {
  PhysicalRect snapshot_containing_block_rect(
      PhysicalOffset(style_tracker.GetFixedToSnapshotRootOffset()),
      PhysicalSize(style_tracker.GetSnapshotRootSize()));

  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          GetDocument().GetLayoutView()->StyleRef(), EDisplay::kBlock);
  new_style_builder.SetPosition(EPosition::kFixed);
  new_style_builder.SetLeft(Length::Fixed(snapshot_containing_block_rect.X()));
  new_style_builder.SetTop(Length::Fixed(snapshot_containing_block_rect.Y()));
  new_style_builder.SetWidth(
      Length::Fixed(snapshot_containing_block_rect.Width()));
  new_style_builder.SetHeight(
      Length::Fixed(snapshot_containing_block_rect.Height()));
  new_style_builder.SetPointerEvents(EPointerEvents::kNone);

  SetStyle(new_style_builder.TakeStyle());
}

}  // namespace blink
