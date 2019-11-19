// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Nearly-complete implementation of DocumentMarkerList for text match or text
// fragment markers (subclassed by TextMatchMarkerListImpl and
// TextFragmentMarkerListImpl to implement the MarkerType() method).
class CORE_EXPORT TextMarkerBaseListImpl : public DocumentMarkerList {
 public:
  // DocumentMarkerList implementations
  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  DocumentMarker* FirstMarkerIntersectingRange(unsigned start_offset,
                                               unsigned end_offset) const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      unsigned start_offset,
      unsigned end_offset) const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool ShiftMarkers(const String& node_text,
                    unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  void Trace(Visitor*) override;

 protected:
  TextMarkerBaseListImpl() = default;
  HeapVector<Member<DocumentMarker>> markers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextMarkerBaseListImpl);
};

template <>
struct DowncastTraits<TextMarkerBaseListImpl> {
  static bool AllowFrom(const DocumentMarkerList& list) {
    return list.MarkerType() == DocumentMarker::kTextMatch ||
           list.MarkerType() == DocumentMarker::kTextFragment;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_TEXT_MARKER_BASE_LIST_IMPL_H_
