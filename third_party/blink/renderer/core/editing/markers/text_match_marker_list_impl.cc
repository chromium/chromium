// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_match_marker_list_impl.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

DocumentMarker::MarkerType TextMatchMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextMatch;
}

static void UpdateMarkerLayoutRect(const Node& node, TextMatchMarker& marker) {
  DCHECK(node.GetDocument().GetFrame());
  LocalFrameView* frame_view = node.GetDocument().GetFrame()->View();

  DCHECK(frame_view);

  // If we have a locked ancestor, then the only reliable place to have a marker
  // is at the locked root rect, since the elements under a locked root might
  // not have up-to-date layout information.
  if (auto* locked_root =
          DisplayLockUtilities::HighestLockedInclusiveAncestor(node)) {
    if (auto* locked_root_layout_object = locked_root->GetLayoutObject()) {
      marker.SetRect(frame_view->FrameToDocument(
          PhysicalRect(locked_root_layout_object->AbsoluteBoundingBoxRect())));
    } else {
      // If the locked root doesn't have a layout object, then we don't have the
      // information needed to place the tickmark. Set the marker rect to an
      // empty rect.
      marker.SetRect(PhysicalRect());
    }
    return;
  }

  const Position start_position(node, marker.StartOffset());
  const Position end_position(node, marker.EndOffset());
  EphemeralRange range(start_position, end_position);

  marker.SetRect(
      frame_view->FrameToDocument(PhysicalRect(ComputeTextRect(range))));
}

Vector<IntRect> TextMatchMarkerListImpl::LayoutRects(const Node& node) const {
  Vector<IntRect> result;

  for (DocumentMarker* marker : markers_) {
    auto* const text_match_marker = To<TextMatchMarker>(marker);
    if (!text_match_marker->IsValid())
      UpdateMarkerLayoutRect(node, *text_match_marker);
    if (!text_match_marker->IsRendered())
      continue;
    result.push_back(PixelSnappedIntRect(text_match_marker->GetRect()));
  }

  return result;
}

bool TextMatchMarkerListImpl::SetTextMatchMarkersActive(unsigned start_offset,
                                                        unsigned end_offset,
                                                        bool active) {
  bool doc_dirty = false;
  auto* const start = std::upper_bound(
      markers_.begin(), markers_.end(), start_offset,
      [](size_t start_offset, const Member<DocumentMarker>& marker) {
        return start_offset < marker->EndOffset();
      });
  for (auto* it = start; it != markers_.end(); ++it) {
    DocumentMarker& marker = **it;
    // Markers are returned in order, so stop if we are now past the specified
    // range.
    if (marker.StartOffset() >= end_offset)
      break;
    To<TextMatchMarker>(marker).SetIsActiveMatch(active);
    doc_dirty = true;
  }
  return doc_dirty;
}

}  // namespace blink
