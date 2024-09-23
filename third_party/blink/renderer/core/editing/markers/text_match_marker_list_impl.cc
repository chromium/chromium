// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_match_marker_list_impl.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

DocumentMarker::MarkerType TextMatchMarkerListImpl::MarkerType() const {
  return DocumentMarker::kTextMatch;
}

bool TextMatchMarkerListImpl::IsEmpty() const {
  return markers_.empty();
}

void TextMatchMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(marker->GetType(), MarkerType());
  SortedDocumentMarkerListEditor::AddMarkerWithoutMergingOverlapping(&markers_,
                                                                     marker);
}

void TextMatchMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>& TextMatchMarkerListImpl::GetMarkers()
    const {
  return markers_;
}

DocumentMarker* TextMatchMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
TextMatchMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                  unsigned end_offset) const {
  return SortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool TextMatchMarkerListImpl::MoveMarkers(int length,
                                          DocumentMarkerList* dst_list) {
  return SortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                     dst_list);
}

bool TextMatchMarkerListImpl::RemoveMarkers(unsigned start_offset, int length) {
  return SortedDocumentMarkerListEditor::RemoveMarkers(&markers_, start_offset,
                                                       length);
}

bool TextMatchMarkerListImpl::ShiftMarkers(const String&,
                                           unsigned offset,
                                           unsigned old_length,
                                           unsigned new_length) {
  return SortedDocumentMarkerListEditor::ShiftMarkersContentDependent(
      &markers_, offset, old_length, new_length);
}

void TextMatchMarkerListImpl::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
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

Vector<gfx::Rect> TextMatchMarkerListImpl::LayoutRects(const Node& node) const {
  Vector<gfx::Rect> result;

  for (DocumentMarker* marker : markers_) {
    auto* const text_match_marker = To<TextMatchMarker>(marker);
    if (!text_match_marker->IsValid())
      UpdateMarkerLayoutRect(node, *text_match_marker);
    if (!text_match_marker->IsRendered())
      continue;
    result.push_back(ToPixelSnappedRect(text_match_marker->GetRect()));
  }

  return result;
}

bool TextMatchMarkerListImpl::SetTextMatchMarkersActive(unsigned start_offset,
                                                        unsigned end_offset,
                                                        bool active) {
  bool doc_dirty = false;
  auto const start = std::upper_bound(
      markers_.begin(), markers_.end(), start_offset,
      [](size_t start_offset, const Member<DocumentMarker>& marker) {
        return start_offset < marker->EndOffset();
      });
  auto start_position =
      base::checked_cast<wtf_size_t>(start - markers_.begin());
  auto num_to_adjust = markers_.size() - start_position;
  auto sub_span = base::span(markers_).subspan(start_position, num_to_adjust);
  for (DocumentMarker* marker : sub_span) {
    // Markers are returned in order, so stop if we are now past the specified
    // range.
    if (marker->StartOffset() >= end_offset) {
      break;
    }
    To<TextMatchMarker>(marker)->SetIsActiveMatch(active);
    doc_dirty = true;
  }
  return doc_dirty;
}

}  // namespace blink
